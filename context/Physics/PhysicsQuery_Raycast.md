# Unity::PhysicsQuery::Raycast

**Source:** `Modules/Physics/PhysicsQuery.cpp` (native header: `PhysicsQuery.h`)

Called from `PhysicsScene_CUSTOM_Internal_Raycast_Injected` [see PhysicsScene_Internal_Raycast_Injected.md].
This is the actual physics query implementation — wraps PhysX `PxScene::raycast()`.

---

## Pseudocode — Android (armeabi-v7a)

```cpp
// sub_6441A0 — thin wrapper, rearranges args and calls inner implementation
int __fastcall Unity::PhysicsQuery::Raycast(int a1, int a2, int a3, float a4, int a5, int a6, int a7)
{
  return sub_643F00(a5, a2, a3, 0, a4, a5, a6, a7);
}
```

Parameter mapping (Android):

| Param | Type | Description |
|---|---|---|
| `a1` | `PhysicsQuery*` | this — `PhysicsManager + 0x68` |
| `a2` | `int` | `PhysicsScene::m_Handle` (scene index) |
| `a3` | `int` | `Ray*` — `{Vector3 origin +0x00, Vector3 direction +0x0C}` (24 bytes) |
| `a4` | `float` | `maxDistance` |
| `a5` | `int` | `RaycastHit*` — output struct; `NULL` for `RaycastTest` variant |
| `a6` | `int` | `layerMask` |
| `a7` | `int` | `queryTriggerInteraction` — 0=Ignore, 1=UseGlobal, 2=Collide |

---

## Pseudocode — Engine (x64)

```cpp
// Unity::PhysicsQuery::Raycast — full implementation (no inner function split)
__int64 __fastcall Unity::PhysicsQuery::Raycast(
    __int64 a1,                 // rcx: PhysicsQuery* (this), at PhysicsManager + 0xB0
    int a2,                     // rdx: PhysicsScene::m_Handle (scene index)
    __int64 a3,                 // r8:  Ray* — {origin +0x00, direction +0x0C}
    __int64 a4,                 // r9:  maxDistance (float, passed as __int64 register)
    struct RaycastHit *a5,      // stack: RaycastHit* output (NULL for RaycastTest)
    unsigned int a6,            // stack: layerMask
    int a7)                     // stack: queryTriggerInteraction
{
  // 1. Assert: direction must be normalized (editor-only, non-fatal)
  // |direction|^2 - 1 > 1e-5 → logs "IsNormalized(ray.GetDirection())" assertion
  if ( fabsf(dot(direction, direction) - 1.0f) > 0.0000099999997f )
    DebugStringToFile("Assertion failed on expression: 'IsNormalized(ray.GetDirection())'");

  // 2. Profiling
  profiler_begin_object(&unk_144667220, 0);   // "Physics.Raycast" profiler marker

  // 3. Get PxScene — returns NULL if scene handle invalid
  GetPhysicsManager();
  v9 = PhysicsManager::GetPhysicsScene(a2);  // sub_65C404 on Android
  if ( !v9 ) { v10 = 0; goto done; }

  // 4. Sync transforms before query
  v11 = GetPhysicsManager();
  PhysicsManager::AutoSyncTransforms(v11);   // sub_65DC18 on Android

  // 5. Build CastFilter (layerMask + queryTriggerInteraction)
  CastFilter::CastFilter(&v36, 0, /*isSingle=*/1, a6, a7);

  // 6. Setup PxHitBuffer (single-hit, stack-allocated)
  v26 = &physx::PxHitBuffer<physx::PxRaycastHit>::`vftable';
  v27 = 0;  // clears hit result storage

  // 7. Unpack Ray: origin from a3+0x00..0x08, direction from a3+0x0C..0x14
  v20[0..2] = *(float*)(a3 + 0), +4, +8;    // origin  xyz
  v19[0..2] = *(float*)(a3 + 12), +16, +20; // direction xyz

  // 8. PhysX raycast — PxScene vtable slot 94 (vtable + 752 bytes, 8-byte ptrs)
  v13 = *((QWORD*)v9 + 2);  // PxScene* at PhysicsScene (native) + 0x10
  PxScene_vtable_94(v13, v20/*origin*/, v19/*direction*/);

  // 9. Check hit — high 64 bits of v27 (PxRaycastHit) non-zero means hit
  if ( _mm_srli_si128(v27, 8).m128i_u64[0] )
  {
    PxLocationHitToRaycastHit(&v21, a5);       // sub_6424F0 on Android
    // Write UV coords (not in PxLocationHit, stored separately)
    *((DWORD*)a5 + 8) = v23[32];  // a5 + 0x20 = RaycastHit::m_UV.x
    *((DWORD*)a5 + 9) = v23[36];  // a5 + 0x24 = RaycastHit::m_UV.y
    v10 = 1;
  }
  else
    v10 = 0;

done:
  profiler_end(&unk_144667220);
  return v10;
}
```

---

## Android inner function — sub_643F00 (the real implementation)

On Android, `Unity::PhysicsQuery::Raycast` is a thin wrapper. The actual logic lives in `sub_643F00`:

```cpp
// sub_643F00 — Android implementation equivalent of Windows Unity::PhysicsQuery::Raycast
int __fastcall sub_643F00(int a1, int a2, int a3, int a4, float a5, int a6, int a7, int a8)
{
  // Confirmed param mapping by usage in body:
  // a2 = scene_handle   → sub_65C404(a2) = GetPhysicsScene
  // a3 = Ray*           → reads 6 floats from a3[0..20]
  // a4 = 0              → bool flag (v8=a4→v51, always 0 for Raycast; may differ for SphereCast)
  // a5 = float maxDist  → v12 = LODWORD(a5); if (a5 == INF) v12 = 0x7F7FFFFF
  // a6 = RaycastHit*    → sub_6424F0(&v53, a6) writes hit data here
  // a7 = layerMask      → v50[3] = a7 (stored in filter struct)
  // a8 = queryTriggerInteraction → switch(a8) cases 0,1,2

  int v15 = 0;  // return value (hit = 1)

  // 1. GetPhysicsScene
  v11 = sub_65C404(a2);   // PhysicsManager::GetPhysicsScene
  if (!v11) return 0;

  // 2. AutoSyncTransforms
  v13 = GetPhysicsManager();
  sub_65DC18(v13);         // PhysicsManager::AutoSyncTransforms

  // 3. Setup hit result storage (local PhysX hit buffer, stack)
  // v50[0] = (int)off_EF316C  (physx::PxHitBuffer<PxRaycastHit> vftable)
  // v31 = off_EF3154           (physx::PxQueryFilterCallback vftable)

  // 4. maxDistance: infinity → 0x7F7FFFFF (FLT_MAX), otherwise use as-is
  v12 = LODWORD(a5);
  if (a5 == INFINITY) v12 = 2139095039;  // 0x7F7FFFFF = FLT_MAX

  // 5. QueryTriggerInteraction → resolve hitsTriggers flag
  switch (a8)
  {
    case 2:  v14 = 1; break;                                        // Collide
    case 1:  /* v14 unchanged = 0 (false) */ break;                 // UseGlobal → no change
    case 0:  v14 = *(_BYTE*)(GetPhysicsManager() + 52); break;     // Ignore → read global queriesHitTriggers
  }
  v52 = v14;  // stored in CastFilter equivalent (v51/v52 area)

  // 6. Flag from GetPhysicsManager()+120: enables MeshQuery flags (v16 = 15 or 143)
  v17 = *(unsigned __int8*)(GetPhysicsManager() + 120);
  v16 = v17 ? 143 : 15;  // PxHitFlags: 15=eDEFAULT, 143=eDEFAULT|eMESH_MULTIPLE?

  // 7. Unpack Ray: origin at a3+0..8, direction at a3+12..20
  v30[0..2] = *(int*)(a3 + 0), +4, +8;    // origin  xyz
  v29[0..2] = *(int*)(a3 + 12), +16, +20; // direction xyz

  // 8. PhysX raycast — vtable slot 95 (vtable + 380 bytes, 4-byte ptrs)
  v18 = *(DWORD*)(v11 + 8);  // PxScene* at PhysicsScene (native) + 0x08
  v19 = vtable_380(v18, v30, v29, v12, &v31, &v28/*flags*/, v47/*hitBuf*/, v50/*filterData*/, 0);

  // 9. Check hit — HIDWORD(v32) non-zero means a hit was recorded
  if (HIDWORD(v32))
  {
    sub_6424F0((int)&v53, a6);    // PxLocationHitToRaycastHit → fills RaycastHit fields
    v15 = 1;
    // Write UV coords separately (not part of PxLocationHit)
    *(_DWORD*)(a6 + 32) = LODWORD(v61);  // a6 + 0x20 = RaycastHit::m_UV.x
    *(_DWORD*)(a6 + 36) = HIDWORD(v61);  // a6 + 0x24 = RaycastHit::m_UV.y
  }
  return v15;
}
```

---

## Function renames (Android IDB — libunity.so)

| Original | Rename | Reason |
|---|---|---|
| `sub_6441A0` | `Unity::PhysicsQuery::Raycast` | Already confirmed — thin wrapper |
| `sub_643F00` | `Unity::PhysicsQuery::RaycastInternal` | Real implementation; equivalent to Windows body; `a4=0` bool may distinguish from SphereCast/CapsuleCast variant |
| `sub_65C404` | `PhysicsManager::GetPhysicsScene` | Confirmed by engine IDB symbol name |
| `sub_65DC18` | `PhysicsManager::AutoSyncTransforms` | Confirmed by engine IDB symbol name |
| `sub_6424F0` | `PxLocationHitToRaycastHit` | Confirmed by engine IDB symbol; writes Point/Normal/Distance/FaceID/Collider to RaycastHit |

## Global / vtable renames (Android IDB)

| Original | Rename | Reason |
|---|---|---|
| `off_EF316C` | `physx::PxHitBuffer<physx::PxRaycastHit>::vftable` | Confirmed by Windows symbol; used as vtable of stack hit buffer |
| `off_EF3154` | `physx::PxQueryFilterCallback::vftable` | Confirmed by Windows; used as base vftable for CastFilter |

---

## PxScene::raycast vtable offset

| Platform | Vtable offset | Slot# | Notes |
|---|---|---|---|
| armeabi-v7a | `+380` | 95 (380 / 4) | 4-byte function pointers |
| x64 (engine) | `+752` | 94 (752 / 8) | 8-byte function pointers |

One slot difference (95 vs 94) is due to PhysX version or platform-specific virtual table layout.

---

## RaycastHit write pattern (cross-validation)

Both platforms write to the same `RaycastHit` fields [confirmed layout from IL2CPP dump]:

| Field | Offset | Android | Windows |
|---|---|---|---|
| m_Point, m_Normal, m_Distance, m_FaceID, m_Collider | +0x00..+0x2C | `sub_6424F0` (`PxLocationHitToRaycastHit`) | `PxLocationHitToRaycastHit(&v21, a5)` |
| m_UV.x | +0x20 | `*(_DWORD*)(a6 + 32) = LODWORD(v61)` | `*((DWORD*)a5 + 8) = v23[32]` |
| m_UV.y | +0x24 | `*(_DWORD*)(a6 + 36) = HIDWORD(v61)` | `*((DWORD*)a5 + 9) = v23[36]` |

`m_UV` is NOT filled by `PxLocationHitToRaycastHit` — it comes from a separate PhysX hit field stored in `v61` (Android) / `v23[32..36]` (Windows).

---

## PhysicsManager fields accessed

| Offset | Platform | Type | Usage |
|---|---|---|---|
| `+52` (0x34) | armeabi-v7a | `bool` | `Physics.queriesHitTriggers` — global default for `QueryTriggerInteraction.UseGlobal` |
| `+120` (0x78) | armeabi-v7a | `bool` | `Physics.queriesHitBackfaces`? or mesh query mode flag — enables additional PxHitFlags (bit 7: `0x80`) |

---

## Native PhysicsScene struct (native, not managed)

`sub_65C404(scene_handle)` returns a **native** `PhysicsScene` object (C++ class, not the managed `PhysicsScene` struct which is just an int).

| Offset | Platform | Content |
|---|---|---|
| `+0x08` | armeabi-v7a | `PxScene*` — `*(DWORD*)(v11 + 8)` |
| `+0x10` | x64 | `PxScene*` — `*((_QWORD*)v9 + 2)` |

---

## Platform behavior differences

| Behavior | Android | Windows |
|---|---|---|
| Direction normalization check | Not present | Assert `IsNormalized(direction)`, non-fatal (logs only) |
| Profiling markers | Not present | `profiler_begin_object` / `profiler_end` |
| Inner function split | Yes — thin wrapper → `sub_643F00` | No — single function body |
| `a4=0` bool flag | Hardcoded 0 passed to inner function | No equivalent (possibly absorbed by CastFilter constructor) |

---

## Call flow (Android, armeabi-v7a)

```
GetPhysicsManager() + 0x68          → Unity::PhysicsQuery*
  │
  ├─ Unity::PhysicsQuery::Raycast   (sub_6441A0) — thin wrapper
  │     ↓
  ├─ Unity::PhysicsQuery::RaycastInternal  (sub_643F00)
  │     ↓
  ├─ PhysicsManager::GetPhysicsScene(scene_handle)  (sub_65C404)
  │     ↓  returns native PhysicsScene obj (contains PxScene* at +0x08)
  ├─ PhysicsManager::AutoSyncTransforms(PhysicsManager*)  (sub_65DC18)
  │     ↓  syncs pending transform changes before query
  ├─ CastFilter setup  (layerMask→v50[3], QTI→v52, flags→v16/v28)
  │     ↓
  ├─ PxScene vtable[380](PxScene*, origin, direction, maxDist, ...)
  │     ↓  actual PhysX raycast — fills v32..v63 on stack
  └─ if hit:
        PxLocationHitToRaycastHit(&v53, RaycastHit*)  (sub_6424F0)
        + UV write to RaycastHit + 0x20/0x24
```

---

## Notes

- `sub_643F00` likely handles more than just Raycast — the `a4` bool parameter (hardcoded 0 here) and the `a1` parameter (unclear) may distinguish it from SphereCast/CapsuleCast variants (need further investigation)
- `PhysicsManager::AutoSyncTransforms` ensures any pending `Transform.SetPosition` / `SetRotation` writes are propagated to PhysX before the query — critical for accurate results
- The direction normalization assertion (Windows only) means you MUST pass a normalized direction or results are undefined
- `PxLocationHitToRaycastHit` fills: `m_Point`, `m_Normal`, `m_Distance`, `m_FaceID`, `m_Collider` — but NOT `m_UV` (filled separately from a different PhysX hit field)
- For `RaycastTest` variant: `a5 = NULL` (RaycastHit* = 0), `sub_6424F0` is never called, returns bool only
