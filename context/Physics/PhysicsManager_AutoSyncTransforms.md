# PhysicsManager::AutoSyncTransforms / SyncTransforms

**Source (engine):** `Runtime/Dynamics/PhysicsManager.cpp`

Called by `Unity::PhysicsQuery::Raycast` before every physics query to flush pending transform writes to PhysX.

---

## PhysicsManager::AutoSyncTransforms

### Pseudocode — Android (armeabi-v7a)

```cpp
int __fastcall PhysicsManager::AutoSyncTransforms(int a1)
{
  // Read m_AutoSyncTransforms flag at PhysicsManager + 122 (0x7A)
  int result = *(unsigned __int8 *)(a1 + 122);
  if ( result )
    result = PhysicsManager::SyncTransforms();   // sub_65C45C
  return result;
}
```

### Pseudocode — Engine (x64)

```cpp
void __fastcall PhysicsManager::AutoSyncTransforms(PhysicsManager *this)
{
  // Read m_AutoSyncTransforms flag at PhysicsManager + 202 (0xCA)
  if ( *((_BYTE *)this + 202) )
    PhysicsManager::SyncTransforms(this);
}
```

### PhysicsManager flag offset

| Platform    | Offset     | Field                | Description                           |
|-------------|------------|----------------------|---------------------------------------|
| armeabi-v7a | `+122` (0x7A) | `m_AutoSyncTransforms` | `Physics.autoSyncTransforms` bool — if true, sync before every query |
| x64 (engine) | `+202` (0xCA) | `m_AutoSyncTransforms` | Same field, different layout |

> `Physics.autoSyncTransforms` was introduced in Unity 2018.3. Default is `true`. When `false`, transforms changed via `Transform.position` are NOT automatically flushed to PhysX before queries — can cause stale raycast results.

---

## PhysicsManager::SyncTransforms

### Function rename (Android IDB)

| Original    | Rename                           | Reason                            |
|-------------|----------------------------------|-----------------------------------|
| `sub_65C45C` | `PhysicsManager::SyncTransforms` | Confirmed by Windows symbol name |

### High-level description

Two-phase function — iterates dirty transforms via `TransformChangeDispatch` and pushes their new position/rotation/scale to PhysX:

**Phase 1 — Rigidbodies:** Syncs moved/rotated rigidbodies to PhysX actor poses.
**Phase 2 — Colliders:** Syncs moved/scaled colliders to PhysX shape transforms.

---

### Phase 1: Rigidbody sync

```
GetAndClearChangedTransformsForMultipleSystems(
    gTransformChangeDispatch,
    Mask(gBodyChangeHandleT) | Mask(gBodyChangeHandleR) | Mask(gBodyPhysicsAnimationHandle),
    ...
)
→ list of (Transform*, changeMask) pairs
```

For each dirty transform:

1. `Transform → GameObject → QueryComponentByType<Rigidbody>()`
2. Compute `teleport` flag:
   ```cpp
   bool teleport = !hasPhysicsAnimation || !Rigidbody::GetIsKinematic(rb);
   // true  → set pose directly (teleport, no velocity correction)
   // false → smooth push (kinematic body driven by animation)
   ```
3. Dispatch by which change bits are set:

| gBodyChangeHandleR set | gBodyChangeHandleT set | Action |
|---|---|---|
| ✓ | ✓ | `Rigidbody::WritePose(rb, teleport)` — full pose (pos+rot) |
| ✗ | ✓ | `Transform::GetPosition` → `Rigidbody::SetPositionInternal(rb, &pos, teleport, 1)` |
| ✓ | ✗ | `Transform::GetRotation` → `Rigidbody::SetRotationInternal(rb, &rot, teleport, 1)` |
| ✗ | ✗ | (no-op — only physics animation bit set) |

---

### Phase 2: Collider sync

```
GetAndClearChangedTransformsForMultipleSystems(
    gTransformChangeDispatch,
    Mask(gColliderChangeHandle_TR) | Mask(gColliderChangeHandle_S),
    ...
)
→ list of (Transform*, changeMask) pairs
```

For each dirty transform:

1. Walk the component list of the owning `GameObject`
2. For each component whose vtable pointer falls within the collider type range:
   ```cpp
   // vtable slot: Android +172 (slot 43) / Windows +512 (slot 64)
   collider->WritePose(isTRChanged, isScaleChanged, 0);
   ```
   - `isTRChanged` = `Mask(gColliderChangeHandle_TR)` bit set → position/rotation moved
   - `isScaleChanged` = `Mask(gColliderChangeHandle_S)` bit set → scale changed

> Collider type range check (vtable range filter):
> - Android: `*(_DWORD*)component - dword_F09AC8 < (unsigned int)dword_F09ACC`
> - Windows: `*(_DWORD*)component - dword_1445242A0 < (unsigned int)dword_1445242A4`

---

## TransformChangeDispatch system

Unity uses `TransformChangeDispatch` to track which transforms have changed since the last sync. Each subsystem (physics, animation, etc.) registers a `TransformChangeSystemHandle` — an index into a per-transform 64-bit dirty bitmask.

`GetAndClearChangedTransformsForMultipleSystems` returns all transforms with any of the requested bits set, and clears those bits atomically.

---

## Global renames

### Android IDB — `libunity.so`

| Original        | Rename                                   | Reason |
|-----------------|------------------------------------------|--------|
| `dword_F75FD0`  | `gBodyChangeHandleT`                     | TransformChangeSystemHandle for translation — bit tested for SetPositionInternal |
| `dword_F75FD4`  | `gBodyChangeHandleR`                     | TransformChangeSystemHandle for rotation — bit tested for SetRotationInternal |
| `dword_F75FD8`  | `gBodyPhysicsAnimationHandle`            | TransformChangeSystemHandle for physics animation — used in teleport flag |
| `dword_F75FC8`  | `gColliderChangeHandle_S`                | TransformChangeSystemHandle for scale — second bool arg to Collider::WritePose |
| `dword_F75FCC`  | `gColliderChangeHandle_TR`               | TransformChangeSystemHandle for TR — first bool arg to Collider::WritePose |
| `dword_F09AC8`  | `g_ColliderTypeRangeBase`                | Collider vtable range base — used to identify Collider components |
| `dword_F09ACC`  | `g_ColliderTypeRangeSize`                | Collider vtable range size — range check `vtable - base < size` |
| `sub_42A1A8`    | `TransformChangeDispatch::GetAndClearChangedTransformsForMultipleSystems` | Confirmed by Windows symbol |
| `sub_6518EC`    | `Rigidbody::GetIsKinematic`              | Return used in teleport flag computation |
| `sub_655C68`    | `Rigidbody::WritePose`                   | Called when both R and T change bits are set |
| `sub_656420`    | `Rigidbody::SetPositionInternal`         | Called when only T change bit is set; args: `(rb, &pos, teleport, 1)` |
| `sub_656548`    | `Rigidbody::SetRotationInternal`         | Called when only R change bit is set; args: `(rb, &rot, teleport, 1)` |
| `sub_C5EC0`     | `Transform::GetRotation`                 | Reads global rotation before SetRotationInternal |
| `sub_1AA23C`    | `dynamic_array::~dynamic_array` (v58)    | Destructor for the change-list array (transform list) |
| `sub_1AA274`    | `dynamic_array::~dynamic_array` (v62)    | Destructor for the change-list array (mask list) |
| `unk_F082DC`    | `TypeContainer<Rigidbody>::rtti`         | Rigidbody type info — passed to QueryComponentByType equivalent |

> `CalculateGlobalPosition` is already a named IDA function (confirmed by Windows: `Transform::GetPosition`).

### Windows IDB — globals already named by engine

| Symbol | Notes |
|---|---|
| `TransformChangeDispatch::gTransformChangeDispatch` | Global singleton |
| `gBodyChangeHandleT`, `gBodyChangeHandleR`, `gBodyPhysicsAnimationHandle` | Body system handles |
| `gColliderChangeHandle_TR`, `gColliderChangeHandle_S` | Collider system handles |
| `dword_1445242A0`, `dword_1445242A4` | Collider vtable range base/size (unnamed in IDB but same pattern) |

---

## Collider::WritePose vtable slot

| Platform    | Vtable offset | Slot | Notes |
|-------------|---------------|------|-------|
| armeabi-v7a | `+172`        | 43   | 4-byte function pointers |
| x64 (engine)| `+512`        | 64   | 8-byte function pointers |

Slot difference (43 vs 64) — likely due to Unity version / platform build differences in the Collider vtable.

---

## Practical notes

### For injection-based raycast

`PhysicsManager::AutoSyncTransforms` is already called internally by `sub_643F00` (`Unity::PhysicsQuery::RaycastInternal`) before the PhysX query. You do **not** need to call it manually.

### Why this matters

If `m_AutoSyncTransforms` is `false` (i.e., `Physics.autoSyncTransforms = false` in the game), then `AutoSyncTransforms` is a no-op and PhysX may have stale transform data. This can cause raycasts to miss or hit objects at old positions. For a cheat, this is rarely an issue since most games leave this at the default `true`.

### Reading the flag externally (optional)

```cpp
const uint32_t physics_manager = GVBXIReadObject<uint32_t>(gManagerContext + 9 * 4);
const bool auto_sync = GVBXIReadObject<uint8_t>(physics_manager + 122);  // Android
// const bool auto_sync = GVBXIReadObject<uint8_t>(physics_manager + 202);  // Windows
```
