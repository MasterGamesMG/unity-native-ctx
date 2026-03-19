# Transform::get_position

**Signature:** `void GetPosition(Transform* self, Vector3* out)`

## Pseudocode — Android (armeabi-v7a)

```cpp
float __fastcall Transform::GetPosition(int self, Vector3 *out)
{
  int v4; // r0
  double v5; // d16
  float result; // r0
  double v7; // [sp+0h] [bp-20h] BYREF
  float v8; // [sp+8h] [bp-18h]

  if ( !self || !Object::GetCachedPtr(self) )
  {
    RaiseNullReferenceException();
    JUMPOUT(0x3F8308);  // exception handler
  }
  v4 = Object::GetCachedPtr(self);
  CalculateGlobalPosition((int)&v7, v4);
  v5 = v7;
  result = v8;
  out->z = v8;
  *(double *)&out->x = v5;  // writes x and y packed as 8 bytes (ARM)
  return result;
}
```

## Pseudocode — Engine IDB (x64)

```cpp
_DWORD *__fastcall Transform::GetPosition(__int64 a1, _DWORD *a2)
{
  struct JobFence *v4; // rcx
  __m128i *v5; // rax
  __m128 v6; // xmm0
  _DWORD *result; // rax
  __int128 v8; // [rsp+20h] [rbp-28h] BYREF
  char v9[24]; // [rsp+30h] [rbp-18h] BYREF

  v4 = *(struct JobFence **)(a1 + 0x70);
  if ( *(_QWORD *)v4 )
    CompleteFenceInternal(v4);       // wait for pending jobs before reading
  v8 = *(_OWORD *)(a1 + 0x70);
  v5 = (__m128i *)CalculateGlobalPosition(v9, &v8);
  *a2 = *v5;                         // out->x
  a2[1] = _mm_shuffle_epi32(*v5, 85).m128i_u32[0];  // out->y
  v6 = *(__m128 *)v5;
  result = a2;
  a2[2] = _mm_movehl_ps(v6, v6).m128_u32[0];        // out->z
  return result;
}
```

## Reconstructed signature

```cpp
void Transform::GetPosition(Transform* self, Vector3* out);
```

## Function renames (Android IDB)

| Original | Rename | Reason |
|---|---|---|
| `sub_33FCB0` | `Transform::GetPosition` | Registration via `get_position_Injected` |
| `sub_3EC10C` | `Object::GetCachedPtr` | Reads `m_CachedPtr` at offset `+0x08` from any `UnityEngine.Object` managed wrapper — returns the native C++ object pointer. ~3.4k xrefs, not Transform-specific |
| `sub_3EC5C0` | `RaiseNullReferenceException` | Creates and raises a managed NullReferenceException via IL2CPP (`il2cpp_domain_get` + `il2cpp_exception_from_name("System", "NullReferenceException")`) |
| `sub_C5D90`  | `CalculateGlobalPosition` | Directly named in engine IDB — see [CalculateGlobalPosition.md](CalculateGlobalPosition.md) |

## Notes

- `a1` = `Transform*` (this), `a2` = `Vector3*` out — result written by pointer on armeabi-v7a
- `Object::GetCachedPtr` reads `m_CachedPtr` at offset `+0x08` of any managed `UnityEngine.Object` — the IL2CPP managed layout in 32-bit is: `[+0x00 klass*][+0x04 monitor*][+0x08 m_CachedPtr]`
- Called twice: once to validate (non-null check), once to retrieve the native pointer — the double-call pattern is the standard guard used across all `_Injected` bindings
- In x64 engine `GetPosition`, `a1` is already the native Transform object (no managed wrapper involved). `TransformAccessReadOnly` is embedded at `a1 + 0x70` (vs `+0x20` in armeabi-v7a) — different offset, same concept
- `CalculateGlobalPosition` traverses the scene hierarchy to compute world space position (not stored directly)
- `RaiseNullReferenceException` internally calls `il2cpp_domain_get` (via `dword_F26C00`) + `il2cpp_exception_from_name` (via `dword_F26D04`) with `("System", "NullReferenceException")` — it constructs the managed exception object before the jump, it does not throw directly in C++
- `JUMPOUT(0x3F8308)` = shared exception dispatcher (not specific to this function)
