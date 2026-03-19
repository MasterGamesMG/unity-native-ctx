# Camera_CUSTOM_get_projectionMatrix_Injected

**Icall for:** `Camera.projectionMatrix` (C# property getter)
**Source (engine):** `Runtime/Camera/Camera.cpp`

> **Externally relevant** — the projection matrix is needed for world→screen projection (ESP).
> To read it externally, navigate to the native `Camera*` and call/read `Camera::GetProjectionMatrix`.

---

## Pseudocode — Android (armeabi-v7a)

```cpp
__int64 *__fastcall Camera_CUSTOM_get_projectionMatrix_Injected(int a1, _QWORD *a2)
{
  // a1 = managed Camera object (IL2CPP MonoObject*)
  // a2 = Matrix4x4f* output (64 bytes)

  // Null-check managed object and its cached native ptr
  if ( !a1 || !Object::GetCachedPtr(a1) )
  {
    RaiseNullReferenceException();
    JUMPOUT(...);
  }

  int v4 = Object::GetCachedPtr(a1);       // native Camera* from managed object
  __int64 *v5 = sub_2D1940(v4);            // Camera::GetProjectionMatrix(Camera*)
  return sub_831340(v5, a2);               // Matrix4x4f copy → output
}
```

## Pseudocode — Engine (x64)

```cpp
void __fastcall Camera_CUSTOM_get_projectionMatrix_Injected(MonoObject *a1, Matrix4x4f *a2)
{
  // ETW/profiling boilerplate + thread check (omitted)

  if ( !a1 ) goto raise_null;

  // Fast path: read cached native Camera* directly from managed object
  Camera *v8 = (Camera *)*((_QWORD *)a1 + 2);   // *(a1 + 0x10) = native Camera*

  if ( !v8 )
  {
    // Slow path: resolve via instance ID + PPtr lookup
    int v13 = Scripting::GetInstanceIDFor(a1);
    v8 = (Camera *)PPtr<Object>::operator Object *(&v13);
    // type validity check omitted
  }

  // Get projection matrix (returns const Matrix4x4f*)
  const Matrix4x4f *v10 = Camera::GetProjectionMatrix(v8, ...);

  // Copy 64 bytes (4 × 16-byte OWORD rows) to output
  *(_OWORD *)a2        = *(_OWORD *)v10;
  *((_OWORD *)a2 + 1)  = *((_OWORD *)v10 + 1);
  *((_OWORD *)a2 + 2)  = *((_OWORD *)v10 + 2);
  *((_OWORD *)a2 + 3)  = *((_OWORD *)v10 + 3);
}
```

---

## KEY: Managed object → native ptr layout

This is the **universal IL2CPP pattern** for ALL Unity native objects (Camera, Transform, Rigidbody, etc.).

```
IL2CPP managed object layout:
  +0x00  vtable / type pointer   (4 bytes Android / 8 bytes Windows)
  +0x04  monitor (lock)          (4 bytes Android / 8 bytes Windows)
  +0x08  native Unity object*    (Android — 32-bit)
  +0x10  native Unity object*    (Windows — 64-bit)
```

| Platform    | Native ptr location      | How accessed |
|-------------|--------------------------|--------------|
| armeabi-v7a | `managed + 0x08`         | `Object::GetCachedPtr(managed)` |
| x64 (engine)| `managed + 0x10`         | `*((_QWORD *)managed + 2)` |

> **External read:**
> ```cpp
> // Android
> uint32_t native_camera = ReadMem<uint32_t>(managed_camera + 0x08);
> // Windows
> uint64_t native_camera = ReadMem<uint64_t>(managed_camera + 0x10);
> ```

This pattern applies to every Unity Component — Transform, Camera, Collider, Rigidbody, etc.

---

## Matrix4x4f struct

```cpp
struct Matrix4x4f {
    // Column-major (Unity/OpenGL convention)
    // Memory layout: m00,m10,m20,m30, m01,m11,m21,m31, m02,m12,m22,m32, m03,m13,m23,m33
    float m[16];   // 64 bytes total
};
```

Used for world→screen projection in ESP:
```cpp
// Project world position to clip space:
// clip = projMatrix * viewMatrix * worldPos
```

---

## Camera::GetProjectionMatrix

Called by the icall to retrieve the actual matrix. Returns `const Matrix4x4f*`.

> **Needs separate documentation** — the return value is a pointer into the Camera struct (cached field) or computed. The offset within `Camera*` where this matrix lives must be confirmed from `sub_2D1940` (Android) pseudocode.

| Platform    | Function         | Notes |
|-------------|------------------|-------|
| armeabi-v7a | `sub_2D1940`     | Returns `Matrix4x4f*` (into Camera struct or computed) |
| x64         | `Camera::GetProjectionMatrix` | Named symbol — returns `const Matrix4x4f*` |

---

## sub_831340 — Matrix4x4f copy (Android-only standalone)

```cpp
// Android armeabi-v7a — sub_831340
// Copies 64 bytes (Matrix4x4f) from a1 → a2
// Returns a1+4 (IDA artefact — points to second 32-byte block read internally)
__int64 *__fastcall sub_831340(__int64 *a1, _QWORD *a2)
{
  // Read all 8 QWORDs (64 bytes) from src
  __int64 v3  = a1[0]; __int64 v4  = a1[1];
  __int64 v5  = a1[2]; __int64 v6  = a1[3];
  __int64 *result = a1 + 4;          // ← IDA return artefact
  __int64 v7  = result[0]; __int64 v8  = result[1];
  __int64 v9  = result[2]; __int64 v10 = result[3];

  // Write all 8 QWORDs (64 bytes) to dst
  a2[0]=v3; a2[1]=v4; a2[2]=v5; a2[3]=v6;
  a2[4]=v7; a2[5]=v8; a2[6]=v9; a2[7]=v10;
  return result;
}
```

**Windows equivalent:** inlined directly as 4× `OWORD` (128-bit) moves — no separate function.
No Windows symbol exists for this. It is a trivial `Matrix4x4f` copy, equivalent to `memcpy(dst, src, 64)`.

---

## Function renames (Android IDB)

| Original     | Rename                           | Reason |
|--------------|----------------------------------|--------|
| `sub_2D1940` | `Camera::GetProjectionMatrix`    | Confirmed by Windows symbol name |
| `sub_831340` | `Matrix4x4f::CopyTo`             | Standalone copy helper on ARM; inlined on Windows as 4× OWORD mov |

---

## External use: reading the projection matrix

To read the projection matrix externally you need the offset of the cached matrix within the native `Camera*` struct. That requires reversing `Camera::GetProjectionMatrix` (`sub_2D1940`).

Once the offset is known:
```cpp
uint32_t native_camera = ReadMem<uint32_t>(managed_camera + 0x08);  // Android
Matrix4x4f proj = ReadMem<Matrix4x4f>(native_camera + CAMERA_PROJMATRIX_OFFSET);
```

**Next step:** reverse `sub_2D1940` to find `CAMERA_PROJMATRIX_OFFSET`.
