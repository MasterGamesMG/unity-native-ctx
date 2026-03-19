# Transform::set_position

**Signature:** `void SetPosition(Transform* self, const Vector3* position)`

## Pseudocode — Android (armeabi-v7a)

```cpp
float32x4_t *__fastcall Transform::SetPosition(int self, Vector3 *position)
{
  int v4; // r0

  if ( !self || !Object::GetCachedPtr(self) )
  {
    RaiseNullReferenceException();
    JUMPOUT(0x3F8308);  // exception handler
  }
  v4 = Object::GetCachedPtr(self);
  return SetGlobalT(v4, position);
}
```

## Pseudocode — Engine IDB (x64)

```cpp
// Source: C:/buildslave/unity/build/Runtime/Transform/Transform.cpp : 871
void __fastcall Transform::SetPosition(Transform *this, const struct Vector3f *a2)
{
  int v6; // er8

  // NaN / Infinity check on all 3 components
  // 0x7F800000 = IEEE 754 exponent mask for +Infinity
  v6 = *(_DWORD *)a2 & 0x7F800000;
  char v7 = 0;
  if ( v6 != 2139095040 )                                    // x is not Inf/NaN
    v7 = (*((_DWORD *)a2 + 1) & 0x7F800000) != 2139095040   // y is not Inf/NaN
      && (*((_DWORD *)a2 + 2) & 0x7F800000) != 2139095040;  // z is not Inf/NaN

  if ( v7 )
  {
    struct JobFence *v12 = (struct JobFence *)*((_QWORD *)this + 14);  // this + 0x70
    if ( *(_QWORD *)v12 )
      CompleteFenceInternal(v12);                 // wait for pending jobs

    __int128 v24 = *((_OWORD *)this + 7);         // TransformAccessReadOnly at this + 0x70

    // pack Vector3 into __m128: [x, y, z, 0]
    __m128 v23 = _mm_movelh_ps(
      (__m128)*(unsigned __int64 *)a2,            // [x, y]
      (__m128)*((unsigned int *)a2 + 2));         // [z, _]

    if ( (unsigned __int8)SetGlobalT(&v24, &v23) )
      Transform::QueueChanges(this);              // mark hierarchy dirty
  }
  else
  {
    // Debug log: "transform.position assign attempt for '%s' is not valid. Input position is { %s, %s, %s }."
    // DebugStringToFile at Transform.cpp:871 — skipped (error path only)
  }
}
```

## Reconstructed signature

```cpp
void Transform::SetPosition(Transform* self, const Vector3* position);
```

## Function renames (Android IDB)

| Original | Rename | Reason |
|---|---|---|
| `sub_C4AE4` | `SetGlobalT` | Write counterpart of `CalculateGlobalPosition` — confirmed by engine IDB: `SetGlobalT(&access, &packed_vec3)` called from `SetPosition`. Takes native Transform on Android (same ABI pattern as `CalculateGlobalPosition`) — see [SetGlobalT.md](SetGlobalT.md) |

## Engine renames

| Name | Notes |
|---|---|
| `SetGlobalT` | Writes world-space position into the transform hierarchy. Takes `(TransformAccessReadOnly*, __m128*)`. Returns bool — true if the value actually changed (used to gate `QueueChanges`) |
| `Transform::QueueChanges` | Marks the transform hierarchy dirty after a write. Called after any successful `SetGlobal*` |

## Notes

- Android wrapper is identical to `GetPosition`: null-check → `GetCachedPtr` → call inner function
- `a2` on Android is the managed `Vector3*` passed from IL2CPP; `SetGlobalT` receives the native Transform object (`v4`) — same ABI asymmetry as `CalculateGlobalPosition` (Android passes full native object; engine passes `TransformAccessReadOnly` directly at `this + 0x70`)
- Engine packs `Vector3f` into `__m128` via `_mm_movelh_ps`: loads `[x, y]` as 8 bytes then `[z]` as 4 bytes → result = `[x, y, z, 0]`
- The NaN/Infinity check (`& 0x7F800000 == 0x7F800000`) guards against setting invalid world positions — if any component is NaN or Inf, the write is skipped and a `DebugStringToFile` error is logged at `Transform.cpp:871`
- `CompleteFenceInternal` / `JobFence` pattern is the same as `GetPosition` — ensures no pending jobs are reading the hierarchy before writing
- `Transform::QueueChanges` is only called when `SetGlobalT` returns true (i.e., the value actually changed) — avoids unnecessary hierarchy updates
- `sub_C4AE4` (`SetGlobalT`) is fully reversed — see [SetGlobalT.md](SetGlobalT.md) for complete pseudocode, `InverseTransformPosition`, dirty propagation, and `TransformHierarchy` field layout
