# CalculateGlobalPosition

**Symbol (engine IDB):** `?CalculateGlobalPosition@@YA?BU_float3@math@@UTransformAccessReadOnly@@@Z`
**Demangled:** `const math::_float3 CalculateGlobalPosition(TransformAccessReadOnly)`
**Android signature:** `Vector3* CalculateGlobalPosition(Vector3* result, int nativeTransform)`
> Android receives the full native Transform object; `TransformAccessReadOnly` is embedded at `nativeTransform + 0x20`.

> `math::_float3` is Unity's internal 3-component float type (12 bytes, equivalent to Vector3).
> On armeabi-v7a the return is written via output pointer (ABI difference vs x64 return in xmm0).

## Pseudocode — Android (armeabi-v7a)

```cpp
// NOTE: 'nativeTransform' is the full native C++ Transform object (from Object::GetCachedPtr).
//       TransformAccessReadOnly is embedded INSIDE it at offset +0x20 (armeabi-v7a).
//       This differs from the engine (x64) where the function receives TransformAccessReadOnly directly.
Vector3 *__fastcall CalculateGlobalPosition(Vector3 *result, int nativeTransform)
{
  _DWORD *v3; // r0
  _DWORD *v4; // r5
  int v5; // r2
  __int64 v6; // r0
  int v7; // r3
  int v8; // r2
  float32x4_t v9; // q8
  int v10; // r5
  int8x16_t v11; // q0
  float32x4_t v12; // q10
  float32x4_t v13; // q12
  float32x4_t v14; // q11
  float32x4_t v15; // q14
  float32x4_t v16; // q13
  int32x4_t v17; // q1
  float32x4_t v18; // q11
  float32x4_t v19; // q10
  float32x4_t v20; // q2
  float32x4_t v21; // q0
  float32x4_t v22; // q15
  float32x4_t v23; // q13
  float32x4_t v24; // q0
  Vector3 *result_; // r0

  v4 = (_DWORD *)(nativeTransform + 0x20);     // v4 = &TransformAccessReadOnly (address of embedded struct)
  v3 = *(_DWORD **)(nativeTransform + 0x20);   // v3 = TransformAccessReadOnly.hierarchy (TransformHierarchy*)

  if ( *v3 )                                     // if JobFence is active
    CompleteFenceInternal((int)v3);              // wait for pending jobs before reading

  v5 = v4[1];                                    // v5 = TransformAccessReadOnly.index (nativeTransform + 0x24)
  v6 = *(_QWORD *)(*v4 + 0x10);                  // v6 = QWORD at (valid + 0x10): LODWORD=matrixList, HIDWORD=matrixWord
  v7 = 3 * v5;                                   // stride index: Matrix is 0x30 bytes = 3 * 0x10
  v8 = *(_DWORD *)(HIDWORD(v6) + 4 * v5);        // v8 = matrixWord[start] — first parent index
  v9 = *(float32x4_t *)(v6 + 0x10 * v7);         // v9 = matrixList[start].position (Vector4, seed accumulator)

  while ( v8 >= 0 )                              // walk hierarchy upward (negative index = root)
  {
    v10 = v6 + 0x30 * v8;                        // v10 = &matrixList[v8] (Matrix entry, 0x30 bytes each)
    v11 = *(int8x16_t *)(v10 + 0x10);            // v11 = matrix[v8].rotation (Quaternion, 16 bytes)

    // --- NEON: build rotation matrix columns from quaternion ---
    // constants decoded (IEEE 754): confirmed against GLM reference implementation
    v12.n128_u64[0] = 0xC0000000C0000000LL;      // v12 = [-2.0f, -2.0f, 2.0f, 0.0f]  (__glob_floats0_glm)
    v12.n128_u64[1] = 0x40000000LL;
    v13 = vmulq_lane_f32(v12, (float32x2_t)v11.n128_u64[0], 1);  // v13 = v12 * rot.y  (comp6 base = [-2,-2,2,0]*rot.y)

    v14.n128_u64[0] = 0xC000000040000000LL;      // v14 = [2.0f, -2.0f, -2.0f, 0.0f]  (__glob_floats1_glm)
    v14.n128_u64[1] = 3221225472LL;              // (3221225472 = 0xC0000000 = -2.0f)
    v15 = vmulq_n_f32(v14, *(float *)(v10 + 0x10));  // v15 = v14 * rot.x  (comp5 base = [2,-2,-2,0]*rot.x)

    v16.n128_u64[0] = 0x40000000C0000000LL;      // v16 = [-2.0f, 2.0f, -2.0f, 0.0f]  (__glob_floats2_glm)
    v16.n128_u64[1] = 3221225472LL;

    // --- quaternion swizzles ---
    v17 = vextq_s8(v11, v11, 8u);               // v17 = rot ZWXY  (extract starting at byte 8 = rotZWXY)
    v8 = *(_DWORD *)(HIDWORD(v6) + 4 * v8);     // v8 = matrixWord[v8] — next parent index (advance before compute)

    v18 = vmulq_n_f32(v14, v11.n128_f32[2]);    // v18 = v14 * rot.z  (comp2 base = [2,-2,-2,0]*rot.z)
    v19 = vmulq_n_f32(v12, v11.n128_f32[2]);    // v19 = v12 * rot.z  (comp4 base = [-2,-2,2,0]*rot.z)
    v20 = vrev64q_s32(v17);                     // v20 = rot WZYX  (reverse within 64-bit pairs of v17)
    v21 = vrev64q_s32(v11);                     // v21 = rot YXWZ  (reverse within 64-bit pairs of rot)

    v22 = vmulq_f32(v21, vmulq_n_f32(v16, *(float *)(v10 + 0x10)));          // v22 = rotYXWZ * (v16 * rot.x) = comp3*rotYXWZ
    v23 = vmulq_f32(v21, vmulq_lane_f32(v16, *(float32x2_t *)(v10 + 0x10), 1)); // v23 = rotYXWZ * (v16 * rot.y) = comp1*rotYXWZ

    // --- apply parent scale to accumulator ---
    v24 = vmulq_f32(v9, *(float32x4_t *)(v10 + 0x20));  // v24 = v9 * TransformNode[v8].scale

    // --- GLM equivalent:
    //   comp1 = (v16*rot.y)*rotYXWZ - (v14*rot.z)*rotZWXY   → *= result[0] (scaled.x)
    //   comp3 = (v12*rot.z)*rotWZYX - (v16*rot.x)*rotYXWZ   → *= result[1] (scaled.y)
    //   comp6 = (v14*rot.x)*rotZWXY - (v12*rot.y)*rotWZYX   → *= result[2] (scaled.z)
    //   result += comp1 + comp3 + comp6 + TransformNode.position
    // ---
    v9 = vaddq_f32(
           *(float32x4_t *)v10,                 // + TransformNode[v8].position
           vaddq_f32(
             vaddq_f32(
               v24,                             // scaled accumulator
               vmulq_n_f32(vsubq_f32(v23, vmulq_f32(v17, v18)), v24.n128_f32[0])  // comp1 * scaled.x
             ),
             vaddq_f32(
               vmulq_lane_f32(vsubq_f32(vmulq_f32(v20, v19), v22), (float32x2_t)v24.n128_u64[0], 1), // comp6 * scaled.y
               vmulq_n_f32(vsubq_f32(vmulq_f32(v17, v15), vmulq_f32(v20, v13)), v24.n128_f32[2])     // comp3 * scaled.z
             )
           )
         );
  }

  // --- write world position to output ---
  *(_QWORD *)&result->x = v9.n128_u64[0];       // result->x and result->y packed (single STRD)
  result_ = (Vector3 *)&result->z;
  LODWORD(result->z) = v9.n128_u32[2];          // result->z
  return result_;
}
```

## Pseudocode — Engine IDB (x64)

```cpp
__m128 *__fastcall CalculateGlobalPosition(__m128 *result, TransformAccessReadOnly *access)
{
  // note: IDA infers 'unsigned int*' by default — actual type confirmed by engine symbol
  __int64 v2; // r8
  __int64 v4; // r11
  __int64 v5; // rdx
  int v6; // eax
  __int64 v7; // rcx
  __int64 v8; // rax
  __m128i v9; // xmm0
  __m128 v10; // xmm11
  __m128 v11; // xmm3
  __m128 v12; // xmm7
  __m128 v13; // xmm9
  __m128 v14; // xmm10
  __m128 v15; // xmm8
  __m128 v16; // xmm4
  __m128 v17; // xmm6

  v2 = a2[2];                                              // TransformAccessReadOnly.index (at +0x08 in x64 struct)
  v4 = *(_QWORD *)(*(_QWORD *)a2 + 0x18i64);              // nodes          (hierarchy + 0x18 in x64)
  v5 = *(_QWORD *)(*(_QWORD *)a2 + 0x20i64);              // parentIndices  (hierarchy + 0x20 in x64)
  v6 = *(_DWORD *)(v5 + 4 * v2);                          // parentIndices[start] = first parent index

  for ( *result = *(__m128 *)(v4 + 48 * v2); v6 >= 0; *result = v17 )  // seed with local position
  {
    v7 = v6;
    v8 = 6i64 * v6;                                      // stride: 6 * 8 bytes = 0x30 per Matrix (x64)
    v9 = *(__m128i *)(v4 + 8 * v8 + 0x10);              // matrix[v6].rotation
    v10 = _mm_mul_ps(*(__m128 *)(v4 + 8 * v8 + 0x20), *result);  // scale * accumulator

    // --- SSE: same quaternion rotation as NEON above, via shuffle ---
    v11 = (__m128)_mm_shuffle_epi32(v9, 0);
    v12 = (__m128)_mm_shuffle_epi32(v9, 113);
    v13 = (__m128)_mm_shuffle_epi32(v9, 142);
    v14 = (__m128)_mm_shuffle_epi32(v9, 85);
    v15 = (__m128)_mm_shuffle_epi32(v9, 170);
    v16 = (__m128)_mm_shuffle_epi32(v9, 219);

    v17 = _mm_add_ps(
            _mm_add_ps(
              _mm_add_ps(
                _mm_mul_ps(
                  _mm_sub_ps(_mm_mul_ps(_mm_mul_ps(v11, (__m128)_xmm), v13),
                             _mm_mul_ps(_mm_mul_ps(v14, (__m128)_xmm), v16)),
                  (__m128)_mm_shuffle_epi32((__m128i)v10, 170)),
                _mm_mul_ps(
                  _mm_sub_ps(_mm_mul_ps(_mm_mul_ps(v15, (__m128)_xmm), v16),
                             _mm_mul_ps(_mm_mul_ps(v11, (__m128)_xmm), v12)),
                  (__m128)_mm_shuffle_epi32((__m128i)v10, 85))),
              _mm_add_ps(
                _mm_mul_ps(
                  _mm_sub_ps(_mm_mul_ps(_mm_mul_ps(v14, (__m128)_xmm), v12),
                             _mm_mul_ps(_mm_mul_ps(v15, (__m128)_xmm), v13)),
                  (__m128)_mm_shuffle_epi32((__m128i)v10, 0)),
                v10)),
            *(__m128 *)(v4 + 8 * v8));                   // + matrix[v6].position
    v6 = *(_DWORD *)(v5 + 4 * v7);                       // next parent index
  }
  return result;
}
```

## Reconstructed signature

```cpp
// engine (x64) — receives TransformAccessReadOnly directly
const math::_float3 CalculateGlobalPosition(TransformAccessReadOnly access);

// android (armeabi-v7a) — receives full native Transform object; TransformAccessReadOnly at +0x20
Vector3* CalculateGlobalPosition(Vector3* result, int nativeTransform);
```

## Internal struct layout

### TransformAccessReadOnly — confirmed by engine symbol

From UnityCsReference (`TransformAccess` layout, sequential):

| Offset (32-bit) | Type | Name | Notes |
|---|---|---|---|
| `+0x00` | `TransformHierarchy*` | `hierarchy` | pointer to the transform hierarchy data block |
| `+0x04` | `int` | `index` | index of this transform within the hierarchy |

In Android pseudocode: `v3 = *v4` = `access.hierarchy`, `v5 = v4[1]` = `access.index`

### TransformHierarchy (pointed to by `access.hierarchy`)

Offsets differ by architecture and Unity version:

| Offset | Arch | Type | Name | Source |
|---|---|---|---|---|
| `+0x00` | both | `JobFence` | `fence` | Android pseudocode `*v3` check |
| `+0x10` | armeabi-v7a (2018.4.11f1) | `TransformNode*` | `nodes` | Android `*(_QWORD *)(*v4 + 0x10)` LODWORD; gamesdk.cpp `valid + 0x10` |
| `+0x14` | armeabi-v7a (2018.4.11f1) | `int*` | `parentIndices` | Android `*(_QWORD *)(*v4 + 0x10)` HIDWORD; gamesdk.cpp `valid + 0x14` |
| `+0x18` | x64 (engine IDB) | `TransformNode*` | `nodes` | Engine `*(_QWORD *)(*(_QWORD *)a2 + 0x18)` |
| `+0x20` | x64 (engine IDB) | `int*` | `parentIndices` | Engine `*(_QWORD *)(*(_QWORD *)a2 + 0x20)` |

### TransformNode (0x30 bytes per entry)

| Offset | Type | Name |
|---|---|---|
| `+0x00` | `Vector4` | `position` |
| `+0x10` | `Quaternion` | `rotation` |
| `+0x20` | `Vector4` | `scale` |

> Struct names (`TransformHierarchy`, `TransformNode`) are not in the symbol — they are reconstructed.
> Personal implementations may name these differently (e.g. "TelemetryObj", "Matrix") — the layout is what matters.

## Function renames (Android IDB)

| Original | Rename | Reason |
|---|---|---|
| `sub_4AA310` | `CompleteFenceInternal` | Same JobFence pattern as engine IDB |

## Notes

- `v6` is a `__int64` packing two 32-bit pointers via a single `LDRD`: `LODWORD` = `nodes`, `HIDWORD` = `parentIndices`
- Engine IDB receives the native Transform's `TransformAccessReadOnly` extracted at `a1 + 0x70`; Android receives the full native Transform and accesses `TransformAccessReadOnly` at `+0x20` internally — same data, different entry point and offset
- `parentIndices[i] < 0` = root node, terminates the loop
- The NEON/SSE block is a quaternion sandwich `q * v * q⁻¹` fused with scale — mathematically identical across both platforms
- Output `result->x` and `result->y` are written packed as a single 64-bit store (`STRD` on ARM)

---

## References

- **Engine symbol (authoritative):** `?CalculateGlobalPosition@@YA?BU_float3@math@@UTransformAccessReadOnly@@@Z`
  — demangled via [MSVC C++ Name Mangling](https://learn.microsoft.com/en-us/cpp/build/reference/decorated-names)

- **UnityCsReference — TransformAccess layout:**
  https://github.com/Unity-Technologies/UnityCsReference/blob/master/Runtime/Transform/ScriptBindings/TransformAccessArray.bindings.cs

- **Unity TransformAccessArray internals article:**
  https://medium.com/toca-boca-tech-blog/unitys-transformaccessarray-internals-and-best-practices-2923546e0b41

- **Unity.Mathematics / math::_float3:**
  https://docs.unity3d.com/Packages/com.unity.mathematics@1.2/api/Unity.Mathematics.float3.html
