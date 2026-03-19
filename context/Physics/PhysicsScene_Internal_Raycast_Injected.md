# PhysicsScene::Internal_Raycast_Injected / Internal_RaycastTest_Injected

Unity has **two** internal raycast icall variants. Both go through the same native path:
`GetPhysicsManager()` → `Unity::PhysicsQuery::Raycast`.

| Variant | Has RaycastHit output | Use case |
|---|---|---|
| `Internal_Raycast_Injected` | Yes (`ref RaycastHit`) | Need hit point / normal / collider |
| `Internal_RaycastTest_Injected` | No | Visibility check only (lighter, preferred for ESP) |

---

## Official C# signatures (Unity 2018.4)

```csharp
// With hit info
private static bool Internal_Raycast(
    PhysicsScene physicsScene,
    Ray         ray,
    float       maxDistance,
    ref RaycastHit hit,
    int         layerMask,
    QueryTriggerInteraction queryTriggerInteraction);

// Bool only (RaycastTest — no hit output)
private static bool Internal_RaycastTest(
    PhysicsScene physicsScene,
    Ray         ray,
    float       maxDistance,
    int         layerMask,
    QueryTriggerInteraction queryTriggerInteraction);
```

Public wrapper (calls `Internal_Raycast` internally):
```csharp
// Unity 2018.4 docs: https://docs.unity3d.com/2018.4/Documentation/ScriptReference/PhysicsScene.Raycast.html
public bool Raycast(
    Vector3 origin,
    Vector3 direction,
    out RaycastHit hitInfo,
    float   maxDistance                = Mathf.Infinity,
    int     layerMask                  = Physics.DefaultRaycastLayers,
    QueryTriggerInteraction queryTriggerInteraction = QueryTriggerInteraction.UseGlobal);
```

Full managed signature string (extracted from IL2CPP stub):
```
UnityEngine.PhysicsScene::Internal_RaycastTest_Injected(
    UnityEngine.PhysicsScene&,
    UnityEngine.Ray&,
    System.Single,
    System.Int32,
    UnityEngine.QueryTriggerInteraction)
```

---

## Pseudocode — Android armeabi-v7a — PhysicsScene_CUSTOM_Internal_Raycast_Injected

```cpp
// Both _Raycast and _RaycastTest share the same wrapper pattern.
// The only difference is sub_6441A0 receives an extra RaycastHit* for _Raycast.
int __fastcall PhysicsScene_CUSTOM_Internal_Raycast_Injected(int *a1, int a2, float a3, int a4, int a5, int a6)
{
  int v10; // r0

  v10 = GetPhysicsManager();
  return sub_6441A0(v10 + 0x68, *a1, a2, a3, a4, a5, a6);
}
```

Parameter mapping (armeabi-v7a):

| Param | Type | Description |
|---|---|---|
| `a1` | `int*` | `PhysicsScene*` — `*a1` is the internal scene handle (int) |
| `a2` | `int` | `Ray*` — `{Vector3 origin, Vector3 direction}` (24 bytes) |
| `a3` | `float` | `maxDistance` |
| `a4` | `int` | `layerMask` |
| `a5` | `int` | `queryTriggerInteraction` — 0=Ignore, 1=UseGlobal, 2=Collide |
| `a6` | `int` | `RaycastHit*` — output struct; absent in `_RaycastTest` variant |

---

## Pseudocode — Engine x64 — PhysicsScene_CUSTOM_Internal_Raycast_Injected

```cpp
__int64 __fastcall PhysicsScene_CUSTOM_Internal_Raycast_Injected(unsigned int *a1, __int64 a2)
{
  // ETW profiling boilerplate (UnityEnableBits & 8) — skipped, error path only
  AssertStackLargeEnough();
  if ( (unsigned int)TlsGetValue(g_ThreadAndSerializationSafeCheckBitField) != 1 )
    ThreadAndSerializationSafeCheck::ReportError("Internal_Raycast", v4);  // must be main thread
  v5 = GetPhysicsManager();
  v6 = Unity::PhysicsQuery::Raycast((char *)v5 + 0xB0, *a1, a2);
  return v6;
}
```

---

## IL2CPP call stub — ARM64 (game binary)

This stub is the IL2CPP-generated bridge that sits **in the game binary** and resolves the icall pointer once, caching it for subsequent calls. It is distinct from `PhysicsScene_CUSTOM_*` which lives in `libunity.so`.

```cpp
// sub_XXXXXX — UnityEngine_PhysicsScene__Internal_RaycastTest_Injected (ARM64, IL2CPP)
__int64 __fastcall UnityEngine_PhysicsScene__Internal_RaycastTest_Injected(
    __int64      a1,            // x0: PhysicsScene& (pointer to scene handle int)
    __int64      a2,            // x1: Ray& (pointer to Ray struct, 24 bytes)
    unsigned int a3,            // x2: System.Single (float maxDistance, passed as uint reg)
    unsigned int a4,            // x3: System.Int32 (layerMask)
    long double  a5)            // x4: QueryTriggerInteraction (enum int — IDA misidentifies as long double)
{
  __int64 (__fastcall *v5)(__int64, __int64, _QWORD, _QWORD, long double); // x4

  v5 = (__int64 (...)*)qword_C398740;  // cached function pointer
  if ( !qword_C398740 )
  {
    v5 = (__int64 (...)*)sub_29A5304(  // il2cpp_resolve_icall — resolve by full signature
      "UnityEngine.PhysicsScene::Internal_RaycastTest_Injected("
      "UnityEngine.PhysicsScene&,UnityEngine.Ray&,"
      "System.Single,System.Int32,UnityEngine.QueryTriggerInteraction)");
    if ( !v5 )
    {
      v11 = sub_29744BC(...);           // get MethodInfo for exception
      sub_2972F94(v11, 0LL, 0LL);      // il2cpp_raise_exception — method not found
      v5 = 0LL;
    }
    qword_C398740 = (__int64)v5;       // cache for next call
  }
  return v5(a1, a2, a3, a4, a5);      // call PhysicsScene_CUSTOM_Internal_RaycastTest_Injected
}
```

### IL2CPP stub global / function renames

| Original | Rename | Reason |
|---|---|---|
| `qword_C398740` | `g_icall_ptr_RaycastTest_Injected` | Cached icall function pointer — first-call resolved, NULL until then |
| `sub_29A5304` | `il2cpp_resolve_icall` | Resolves managed icall by full signature string |
| `sub_29744BC` | `il2cpp_get_method_info` | Gets MethodInfo object (used when resolve fails, for exception) |
| `sub_2972F94` | `il2cpp_raise_exception` | Raises managed exception (MethodNotFoundException if icall not found) |

---

## PhysicsManager offset — PhysicsQuery location

| Platform | Offset | Notes |
|---|---|---|
| armeabi-v7a | `+0x68` | `GetPhysicsManager() + 0x68` → `Unity::PhysicsQuery*` |
| x64 (engine) | `+0xB0` | `(char*)GetPhysicsManager() + 0xB0` → `Unity::PhysicsQuery*` |

---

## Function renames (Android IDB — libunity.so)

| Original | Rename | Reason |
|---|---|---|
| `sub_6441A0` | `Unity::PhysicsQuery::Raycast` | Confirmed by engine IDB symbol |

---

## RaycastHit struct (managed — blittable, same layout in C++)

Confirmed from IL2CPP dump (`TypeDefIndex: 2196`). Fields are the **stored** values — C# properties like `barycentricCoordinate`, `textureCoord2`, `rigidbody`, `transform`, `triangleIndex` are computed on-demand and are **not** stored fields:

```cpp
struct RaycastHit {             // 0x2C bytes (44 bytes) — confirmed IL2CPP dump
    Vector3  m_Point;           // +0x00  impact point world space
    Vector3  m_Normal;          // +0x0C  surface normal
    uint32_t m_FaceID;          // +0x18  face/triangle ID (raw, not triangleIndex)
    float    m_Distance;        // +0x1C  distance from ray origin
    Vector2  m_UV;              // +0x20  primary UV coords (textureCoord property)
    int32_t  m_Collider;        // +0x28  managed Collider handle (IL2CPP object ID)
};                              // total: 0x2C
```

> **Key corrections vs estimated layout**: `distance` is at `+0x1C` not `+0x18`; `m_FaceID` (uint, not `triangleIndex`) sits at `+0x18`; `m_UV` is at `+0x20` not `+0x28`; `rigidbody`/`transform` are **not stored** — retrieved via `m_Collider` handle at runtime. Total size is 0x2C (44 bytes), not 0x50.

From Unity 2018.4 docs ([RaycastHit](https://docs.unity3d.com/2018.4/Documentation/ScriptReference/RaycastHit.html)).

> **Note:** Exact field order and padding must be confirmed against the IDB. The offsets above are derived from the documented field sizes and typical Unity struct packing.

---

## Call chain summary

```
[Game C# code]  PhysicsScene.RaycastTest(...)
      ↓  IL2CPP generated (game binary / ARM64)
UnityEngine_PhysicsScene__Internal_RaycastTest_Injected
      ↓  icall pointer resolved by il2cpp_resolve_icall
PhysicsScene_CUSTOM_Internal_RaycastTest_Injected  (libunity.so)
      ↓  GetPhysicsManager() → PhysicsManager + 0x68
Unity::PhysicsQuery::Raycast  (sub_6441A0)  [see PhysicsQuery_Raycast.md]
      ↓
PhysX: PxScene::raycast(...)
```

---

## Notes

- For a **visibility check (ESP)**: use `Internal_RaycastTest_Injected` — no `RaycastHit` allocation, lighter, only returns bool
- For **hit position / collider info**: use `Internal_Raycast_Injected` — populates full `RaycastHit`
- `PhysicsScene` in Unity 2018 is a plain struct: `struct PhysicsScene { int handle; }` — `*a1` gives the scene index
- `ThreadAndSerializationSafeCheck` on the engine ensures this is called from the main thread — physics queries in Unity 2018 are not thread-safe
- `long double` for `a5` in the IL2CPP stub is an **IDA artifact** — register `x4` holds a 32-bit `QueryTriggerInteraction` enum int; IDA picks up `long double` due to ARM64 register heuristics
- `qword_C398740` starts as NULL and is set on the first call — thread safety of this caching is not guaranteed (Unity assumes single-threaded managed execution)

---

## References

- [Unity 2018.4 — PhysicsScene.Raycast](https://docs.unity3d.com/2018.4/Documentation/ScriptReference/PhysicsScene.Raycast.html)
- [Unity 2018.4 — RaycastHit](https://docs.unity3d.com/2018.4/Documentation/ScriptReference/RaycastHit.html)
- [Unity 2018.4 — QueryTriggerInteraction](https://docs.unity3d.com/2018.4/Documentation/ScriptReference/QueryTriggerInteraction.html)
- [Unity 2018.4 — Physics.Raycast](https://docs.unity3d.com/2018.4/Documentation/ScriptReference/Physics.Raycast.html)
