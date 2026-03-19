# SetGlobalT

**Android signature:** `float32x4_t* SetGlobalT(int nativeTransform, Vector3* position)`
**Engine signature:** `char SetGlobalT(TransformAccessReadOnly* access, __m128* position)` → returns bool

> Write counterpart of `CalculateGlobalPosition`. Converts a world-space position to local space
> and writes it into `nodes[index].position`, then propagates dirty flags through the hierarchy.

> ABI difference (same pattern as `CalculateGlobalPosition`): Android receives the full native
> Transform object and reads `TransformAccessReadOnly` internally at `+0x20`; engine receives
> `TransformAccessReadOnly` directly (packed as `__m128i`: `i64[0]` = hierarchy pointer, `i32[2]` = index).
> On Android, `QueueChanges` is inlined inside this function; on engine it returns `bool` and
> the caller (`SetPosition`) calls `Transform::QueueChanges` externally.

---

## Pseudocode — Android (armeabi-v7a)

```cpp
// NOTE: 'nativeTransform' is the full native C++ Transform object.
//       TransformAccessReadOnly is embedded INSIDE it at offset +0x20 (armeabi-v7a).
float32x4_t *__fastcall SetGlobalT(int nativeTransform, Vector3 *position)
{
  float32x4_t v2;       // q0  — position packed as float32x4
  _DWORD *v4;           // r0  — hierarchy pointer (TransformHierarchy*)
  int v5;               // r5  — &TransformAccessReadOnly (nativeTransform + 0x20)
  _DWORD *v6;           // r4  — hierarchy pointer
  int v7;               // r7  — TransformAccessReadOnly.index
  float32x4_t v29;      // [sp+10h] — working copy of position

  v5 = nativeTransform + 0x20;               // &TransformAccessReadOnly
  v4 = *(_DWORD **)(nativeTransform + 0x20); // hierarchy (first field of TransformAccessReadOnly)

  if ( *v4 )                                 // if JobFence active
    CompleteFenceInternal((int)v4);          // wait for pending jobs before writing

  // Pack input Vector3 into float32x4 working buffer
  v2.n128_u64[0] = *(_QWORD *)&position->x; // load x, y as 8 bytes
  v6 = *(_DWORD **)v5;                       // hierarchy pointer
  v7 = *(_DWORD *)(v5 + 4);                 // TransformAccessReadOnly.index (nativeTransform + 0x24)
  v2.n128_u32[2] = LODWORD(position->z);    // load z
  v29 = [x, y, z, _];                       // working copy

  if ( v7 )   // index != 0 → not the root, must convert world → local
  {
    // v6[5] = *(hierarchy + 0x14) = parentIndices (armeabi-v7a)
    // parentIndices[index] = parent's index in nodes[]
    InverseTransformPosition((int)v6, *(_DWORD *)(v6[5] + 4 * v7), &v29);
    v2 = v29;
  }

  // nodes[index].position — v6[4] = *(hierarchy + 0x10) = nodes; stride = 0x30
  float32x4_t *result = (float32x4_t *)(v6[4] + 0x30 * v7);

  // NEON component-wise equality: v9 = all-ones per lane where equal, 0 where different
  float32x4_t v9 = vceqq_f32(v2, *result);
  *result = v2;   // write new local position to nodes[index].position

  // If any of the 3 components changed: propagate dirty flags and queue changes
  // ~v9 = all-ones where DIFFERENT; vpmin/vmin collapse to scalar; < 0 = any difference
  if ( vmin_s32((int32x2_t)~v9.n128_u64[1], vpmin_s32((int32x2_t)~v9.n128_u64[0], ...)).n64_i32[0] < 0 )
  {
    // Dirty bit propagation — internal bookkeeping (not needed for external read).
    // Propagates flags upward toward hierarchy root and downward to all children.
    // TransformHierarchy fields accessed (armeabi-v7a, _DWORD *v6):
    //   v6[10] = +0x28   global dirty flags (OR'd with new bits)
    //   v6[11] = +0x2C   global dirty flags (high word, 32-bit arch)
    //   v6[12] = +0x30 \ dirty mask constants
    //   v6[13] = +0x34 |
    //   v6[14] = +0x38 |
    //   v6[15] = +0x3C /
    //   v6[20] = +0x50   per-node dirty state array
    //   v6[21] = +0x54   per-node flags array
    //   v6[26] = +0x68   child index list (downward propagation)

    // Android-inlined equivalent of Transform::QueueChanges:
    // dword_F294B8 = TransformChangeDispatch::gTransformChangeDispatch
    result = (float32x4_t *)sub_429D34((__int64 *)dword_F294B8, v27);
  }

  return result;
}
```

---

## Pseudocode — Engine IDB (x64)

```cpp
// TransformAccessReadOnly packed as __m128i:
//   a1->m128i_i64[0] = hierarchy pointer (8 bytes)
//   a1->m128i_i32[2] = index (4 bytes at offset +8)
// Returns: 1 if position changed, 0 if identical (used by SetPosition to gate QueueChanges)
char __fastcall SetGlobalT(__m128i *a1, __m128 *a2)
{
  __m128 v4;
  __m128i v18;
  __m128 v19;

  // Root check: index == 0 means this is the root transform (no parent to invert through)
  bool is_root = (a1->m128i_i32[2] == 0);
  v4  = *a2;
  v19 = *a2;

  if ( !is_root )
  {
    // Extract index from packed __m128i: shift 8 bytes right, extract lowest 32 bits
    unsigned int index = _mm_cvtsi128_si32(_mm_srli_si128(*a1, 8));  // a1->m128i_i32[2]

    // Build a new TransformAccessReadOnly pointing to the PARENT node:
    //   same hierarchy pointer, but index = parentIndices[this.index]
    // *(hierarchy + 0x20) = parentIndices (x64 offset)
    v18.m128i_i64[0] = a1->m128i_i64[0];                                                   // same hierarchy
    v18.m128i_i32[2] = *(_DWORD *)(*(_QWORD *)(v18.m128i_i64[0] + 0x20) + 4i64 * index);  // parentIndices[index]

    InverseTransformPosition(&v18, &v19);  // convert world → local through parent chain
    v4 = v19;
  }

  // Restore original access, compute &nodes[index].position
  // *(hierarchy + 0x18) = nodes (x64 offset); stride = 0x30
  v18 = *a1;
  unsigned int index = _mm_cvtsi128_si32(_mm_srli_si128(v18, 8));
  __m128 *node_pos = (__m128 *)(*(_QWORD *)(v18.m128i_i64[0] + 0x18) + 0x30i64 * index);

  // Change detection: compare xyz components (mask 0b0111 ignores w lane)
  int changed_mask = _mm_movemask_ps(_mm_cmpneq_ps(*node_pos, v4));
  *node_pos = v4;           // write new local position

  if ( (changed_mask & 7) == 0 )
    return 0;               // no xyz component changed → caller skips QueueChanges

  // Dirty bit propagation (x64, _QWORD *v8 = hierarchy):
  _QWORD *v8 = (_QWORD *)v18.m128i_i64[0];
  unsigned int idx = v18.m128i_u32[2];

  __int64 v10 = *(_QWORD *)(v18.m128i_i64[0] + 0x58);    // v8[11] — dirty mask
  __int64 v11 = *(_QWORD *)(v8[15] + 8i64 * idx)         // per-node flags
              & (v10 | *(_QWORD *)(v18.m128i_i64[0] + 0x50)); // v8[10]

  // Propagate dirty bits to this node's slot and global hierarchy dirty
  *(_QWORD *)(v8[14] + 8i64 * idx) |= v11;   // per-node dirty state array; v8[14] = hierarchy + 0x70 (x64)
  v8[9] |= v11;                                // global dirty at hierarchy + 0x48 (x64)

  // Propagate dirty downward to all children
  unsigned int child_count = *(_DWORD *)(v8[5] + 4 * idx);   // v8[5] = +0x28 = child count per node
  int child_idx = *(_DWORD *)(v8[19] + 4 * idx);             // v8[19] = +0x98 = child index array

  if ( child_count > 1 )
  {
    unsigned int remaining = child_count - 1;
    do
    {
      __int64 child_flags = v10 & *(_QWORD *)(v8[15] + 8i64 * child_idx);
      *(_QWORD *)(v8[14] + 8 * child_idx) |= child_flags;
      v8[9] |= child_flags;
      child_idx = *(_DWORD *)(v8[19] + 4 * child_idx);  // next child
      --remaining;
    }
    while ( remaining );
  }

  return 1;   // changed → caller will call Transform::QueueChanges
}
```

---

## Inner function: `InverseTransformPosition` (sub_CA4B8)

**Engine name confirmed from IDB.**
**Android signature:** `void InverseTransformPosition(int hierarchy, int parentIndex, float32x4_t* inout_pos)`
**Engine signature:** `void InverseTransformPosition(TransformAccessReadOnly* access, __m128* inout_pos)`

> Recursive inverse-TRS walk. Converts a world-space position to local space by climbing
> to the root (recursion), then applying the inverse of each ancestor's TRS on the way back
> down (post-order = root-first application order).

### Inverse TRS formula

```
local = (world − node.position) rotated by conjugate(node.rotation), divided by node.scale
```

- **Conjugate quaternion** = `[-rx, -ry, -rz, rw]` — XOR xyz sign bits, keep w
- **Same shuffle masks** as `CalculateGlobalPosition` (0x00/xxxx, 0x55/yyyy, 0x8E/zwxy, 0xDB/wzyw, 0xAA/zzzz, 0x71/yxwy) applied to the conjugate → inverse rotation
- **Scale reciprocal** via Newton-Raphson with zero-scale guard

### Pseudocode — Engine IDB (x64)

```cpp
__int64 __fastcall InverseTransformPosition(__m128i *a1, __m128 *a2)
{
  // Recurse to root first (post-order: apply root transform last on unwind)
  if ( a1->m128i_i32[2] )  // index != 0 (not root)
  {
    __m128 v20;
    v20.m128_u64[0] = a1->m128i_i64[0];  // same hierarchy
    // *(hierarchy + 0x20) = parentIndices; grandparent = parentIndices[parentIndex]
    v20.m128_i32[2] = *(_DWORD *)(*(_QWORD *)(v20.m128_u64[0] + 0x20) + 4i64 * index);
    InverseTransformPosition((__m128i *)&v20, a2);  // recurse toward root
  }

  // *(hierarchy + 0x18) = nodes (x64); nodes[parentIndex]
  __int64 node = *(_QWORD *)(a1->m128i_i64[0] + 0x18) + 0x30i64 * index;

  __m128 scale  = *(__m128 *)(node + 0x20);               // node.scale
  __m128 v20    = _mm_sub_ps(*a2, *(__m128 *)node);       // world_pos − node.position

  // Scale reciprocal — two Newton-Raphson steps (more precise than Android's one step):
  //   est1 = vrecpe(scale)
  //   est2 = est1 * (2 − scale * est1)     ← step 1
  //   est3 = est2 * (2 − scale * est2)     ← step 2  (engine only)
  __m128 est1 = _mm_rcp_ps(scale);
  __m128 est2 = _mm_mul_ps(est1, _mm_sub_ps(two, _mm_mul_ps(est1, scale)));  // step 1
  __m128 est3 = _mm_mul_ps(est2, _mm_sub_ps(two, _mm_mul_ps(est2, scale)));  // step 2

  // Zero-scale guard: |scale| < epsilon (~4.7e-10) → result = 0 for that lane
  __m128 zero_mask = _mm_srai_epi32(_mm_cmplt_ps(_mm_and_ps(scale, abs_mask), epsilon), 31);
  // NaN guard on reciprocal: est3 == est3 fails for NaN → fallback to est1
  __m128 nan_mask  = _mm_srai_epi32(_mm_cmpeq_ps(est3, est3), 31);
  // safe_inv_scale = 0 where near-zero, else est3 (or est1 as NaN fallback)
  __m128 safe_inv_scale = _mm_andnot_ps(zero_mask,
      _mm_or_ps(_mm_and_ps(nan_mask, est3), _mm_andnot_ps(nan_mask, est1)));

  // Conjugate quaternion: XOR xyz sign bits, keep w unchanged
  // sign_mask = [0x80000000, 0x80000000, 0x80000000, 0x00000000]
  __m128i conj_rot = _mm_xor_si128(_mm_and_si128(sign_mask, ...), *(__m128i *)(node + 0x10));
  // → [-rx, -ry, -rz, rw]

  // Quaternion rotation using SAME shuffles and constants as CalculateGlobalPosition:
  //   shuffles: xxxx(0x00), yyyy(0x55), zwxy(0x8E), wzyw(0xDB), zzzz(0xAA), yxwy(0x71)
  //   constants: mulVec0[-2,2,-2,0], mulVec1[2,-2,-2,0], mulVec2[-2,-2,2,0]
  // (applied to conjugate → inverse rotation)
  __m128i zzzz = _mm_shuffle_epi32(conj_rot, 0xAA); // rot.zzzz
  __m128i yxwy = _mm_shuffle_epi32(conj_rot, 0x71); // rot.yxwy
  __m128i zwxy = _mm_shuffle_epi32(conj_rot, 0x8E); // rot.zwxy
  __m128i xxxx = _mm_shuffle_epi32(conj_rot, 0x00); // rot.xxxx
  __m128i wzyw = _mm_shuffle_epi32(conj_rot, 0xDB); // rot.wzyw
  __m128i yyyy = _mm_shuffle_epi32(conj_rot, 0x55); // rot.yyyy

  __m128 rotated = /* same SSE quaternion rotation block as CalculateGlobalPosition */ ...;

  *a2 = _mm_mul_ps(rotated, safe_inv_scale);  // apply inverse scale
}
```

### Pseudocode — Android (armeabi-v7a)

```cpp
// a1 = hierarchy pointer, a2 = parent index (parentIndices[this.index]), a3 = inout position
void InverseTransformPosition(int hierarchy, int parentIndex, float32x4_t *inout_pos)
{
  if ( parentIndex )  // not root
  {
    // *(hierarchy + 0x14) = parentIndices (armeabi-v7a)
    InverseTransformPosition(hierarchy,
        *(_DWORD *)(*(_DWORD *)(hierarchy + 0x14) + 4 * parentIndex), inout_pos);
  }

  // *(hierarchy + 0x10) = nodes (armeabi-v7a); stride = 0x30
  int node = *(_DWORD *)(hierarchy + 0x10) + 0x30 * parentIndex;

  // Conjugate quaternion (node + 0x10 = rotation):
  // XOR [rx, ry, rz, rw] with [0x80000000, 0x80000000, 0x80000000, 0x00000000]
  // → [-rx, -ry, -rz, rw]
  float32x4_t conj_rot = veorq_s64(*(int64x2_t *)(node + 0x10), sign_mask_xyz);

  // Scale reciprocal — two Newton-Raphson steps (same count as engine):
  //   est1 = vrecpeq_f32(scale)                           — initial 8-bit estimate
  //   est2 = est1 * vrecpsq(scale, est1)                  — step 1 (est1 * (2 − scale*est1))
  //   est3 = est2 * vrecpsq(scale, est2)  [inner vbslq]  — step 2 (est2 * (2 − scale*est2))
  float32x4_t scale = *(float32x4_t *)(node + 0x20);
  float32x4_t est1  = vrecpeq_f32(scale);
  float32x4_t est2  = vmulq_f32(est1, vrecpsq_f32(scale, est1));            // step 1

  // Scale-zero guard: Q13 = VCEQ.F32(scale, 0.0) — all-1s where scale == 0.0 exactly
  // When scale==0: vrecpe gives +Inf, step 2 gives NaN → fallback to est1 (+Inf).
  // This is then caught by the outer epsilon guard (|0| < epsilon → 0), so est1 is never
  // actually used in the final result. The VCEQ guard is a defensive NaN barrier.
  // Inner blend (before outer epsilon gate):
  //   if scale == 0.0: est1 (+Inf — overridden to 0 by outer gate)
  //   else:            est3 = est2 * vrecpsq(scale, est2)   ← step 2
  float32x4_t est3_or_fallback = vbslq_s64(Q13, est1, vmulq_f32(est2, vrecpsq_f32(scale, est2)));

  // Zero-scale guard: if |scale| < epsilon (~0x3089705F ≈ 4.7e-10) → use 0 (avoid huge values)
  // vcgtq_f32(epsilon, |scale|) then vshrq_n_s32(..., 31) = all-1s where near-zero
  float32x4_t safe_inv_scale = vbslq_s64(
      vshrq_n_s32(vcgtq_f32(epsilon, vabsq_f32(scale)), 31),
      vdupq_n_f32(0.f), est3_or_fallback);

  // Inverse TRS: subtract position, apply inverse rotation, divide by scale
  float32x4_t translated = vsubq_f32(*inout_pos, *(float32x4_t *)node);  // − node.position
  *inout_pos = vmulq_f32(
      /* quaternion rotation block same as CalculateGlobalPosition but with conj_rot */,
      safe_inv_scale);
}
```

### Key differences vs `CalculateGlobalPosition`

| | `CalculateGlobalPosition` | `InverseTransformPosition` |
|---|---|---|
| Direction | Local → World | World → Local |
| Quaternion | Direct rotation | Conjugate (`[-rx,-ry,-rz,rw]`) |
| Scale | Multiply | Divide (reciprocal) |
| Position | Add | Subtract |
| Traversal | Iterative while loop | Recursive (post-order → root-first) |
| Called by | `GetPosition` | `SetGlobalT` (non-root only) |
| NR steps (reciprocal) | n/a (no reciprocal) | 2 steps (both platforms) |

---

## `Transform::QueueChanges` — Engine IDB

```cpp
void __fastcall Transform::QueueChanges(Transform *this)
{
  // TransformAccess (= TransformAccessReadOnly) embedded at this + 0x70 (x64)
  const struct TransformAccess *v1 = (TransformAccess *)((char *)this + 0x70);

  if ( !*((_QWORD *)this + 14) )  // *(this + 0x70).hierarchy == null
  {
    // Assertion: IsTransformHierarchyInitialized() — logs to Transform.cpp and returns
    IsNativeTestExpectingAssertionFailure("Assertion failed on expression: 'IsTransformHierarchyInitialized()'");
    DebugStringToFile(...);
    return;
  }

  // Actual queue call:
  TransformChangeDispatch::QueueTransformChangeIfHasChanged(
      TransformChangeDispatch::gTransformChangeDispatch, v1);
}
```

**Android equivalent (inlined in `SetGlobalT`):**
```cpp
sub_429D34((__int64 *)dword_F294B8, v27);
// dword_F294B8    = TransformChangeDispatch::gTransformChangeDispatch
// v27             = &TransformAccessReadOnly (= v5 = nativeTransform + 0x20)
// sub_429D34      = QueueTransformChangeIfHasChanged (thin wrapper, dereferences access ptr)
// sub_429CB4      = QueueTransformChangeIfHasChanged internal (appends hierarchy to queue)
```

---

## Function renames (Android IDB)

| Original | Rename | Reason |
|---|---|---|
| `sub_C4AE4` | `SetGlobalT` | Write counterpart of `CalculateGlobalPosition` — confirmed by engine IDB call in `SetPosition`. Same ABI asymmetry (full native Transform on Android vs TransformAccessReadOnly on engine) |
| `sub_CA4B8` | `InverseTransformPosition` | **Engine name confirmed from IDB.** Recursive inverse-TRS: world → local. Mirrors `CalculateGlobalPosition` with conjugate quaternion, reciprocal scale, subtracted position |
| `sub_429D34` | `TransformChangeDispatch::QueueTransformChangeIfHasChanged` | Engine name confirmed: `Transform::QueueChanges` calls `TransformChangeDispatch::QueueTransformChangeIfHasChanged(gTransformChangeDispatch, access)`. `sub_429D34` is the thin wrapper (dereferences access pointer); `sub_429CB4` is the actual implementation |

## Global rename (Android IDB)

| Original | Rename | Reason |
|---|---|---|
| `dword_F294B8` | `TransformChangeDispatch::gTransformChangeDispatch` | Engine name confirmed from `Transform::QueueChanges`: the global `TransformChangeDispatch` instance passed to `QueueTransformChangeIfHasChanged` |

---

## TransformHierarchy fields — combined layout

> All fields accessed across `SetGlobalT`, `InverseTransformPosition`, `CalculateGlobalPosition`.
> Dirty-state fields are internal bookkeeping only — not needed for external read.

| Field | armeabi-v7a | x64 | Purpose |
|---|---|---|---|
| JobFence | `+0x00` | `+0x00` | Thread sync fence |
| nodes | `+0x10` | `+0x18` | TransformNode array pointer |
| parentIndices | `+0x14` | `+0x20` | int array — parent index per node (-1 = root) |
| childCountPerNode | `+0x18` | `+0x28` | depth/child count array (dirty downward propagation) |
| queueSlotIndex | `+0x24` | unknown | `-1` = not yet in change queue |
| globalDirtyFlags | `+0x28–0x2C` | `+0x48` | accumulated dirty bitmask (global, OR'd on change) |
| dirtyMaskConstants | `+0x30–0x3C` | `+0x50–0x58` | masks used during dirty bit OR operations |
| perNodeDirtyState | `+0x50–0x54` | `+0x70–0x78` | per-node dirty state array + flags array |
| childIndexList | `+0x68` | `+0x98` | child index array (downward dirty propagation) |

## Notes

- The root transform check (`index == 0`) skips `InverseTransformPosition` entirely — for root nodes, world position and local position are identical, so no parent chain inversion is needed.
- `InverseTransformPosition` is recursive (post-order) while `CalculateGlobalPosition` is iterative — both produce the same traversal order (root → leaf), but inversion requires the full chain to be known before processing each level, which recursion provides naturally.
- Both platforms do **two** Newton-Raphson steps for the scale reciprocal. The zero-scale guard (epsilon ≈ 4.7e-10) is identical. The fallback mechanism differs: Android uses `VCEQ.F32(scale, 0.0)` (Q13) to fall back to est1 when scale is exactly 0.0 — but this path is always overridden by the outer epsilon guard, making it a defensive NaN barrier. The engine uses `cmpeq_ps(est3, est3)` (NaN self-comparison fails) as a more general NaN fallback to est1.
- The shuffle masks and `mulVec` constants inside `InverseTransformPosition` are **identical** to `CalculateGlobalPosition` — the conjugate quaternion is the only mathematical difference. Same SIMD code path, different input.
- `Transform::QueueChanges` in the engine asserts `IsTransformHierarchyInitialized()` before queuing — the Android inlined version has no such assertion.
- `sub_42B0F4` / `sub_41FC00` called from `sub_429CB4` are generic array-growth helpers (realloc pattern) — not Transform-specific.
