# Camera::GetProjectionMatrix

**Source (engine):** `Runtime/Camera/Camera.cpp`
**Android rename:** `sub_2D1940` → `Camera::GetProjectionMatrix`

> **Externally relevant** — the projection matrix is **cached** at a fixed offset inside `Camera*`.
> You can read it directly from external memory without calling any function.

---

## KEY: Cached matrix offset

| Platform    | Offset          | External read |
|-------------|-----------------|---------------|
| armeabi-v7a | `Camera + 0x7C` | `ReadMem<Matrix4x4f>(native_camera + 0x7C)` |
| x64 (engine)| `Camera + 0xD0` | `ReadMem<Matrix4x4f>(native_camera + 0xD0)` |

The matrix is recomputed lazily (dirty flag) and then **cached in-place** in the Camera struct. The function returns a pointer directly to that cached location — it never allocates.

**In practice:** Unity recomputes camera matrices every frame during render. Externally reading at runtime always returns a valid/current matrix.

---

## Pseudocode — Android (armeabi-v7a)

```cpp
// sub_2D1940 — Camera::GetProjectionMatrix(Camera *this)
int __fastcall Camera::GetProjectionMatrix(int a1)
{
  // Check dirty flag — if set, recompute and cache the matrix
  if ( *(_BYTE *)(a1 + 0x474) )  // m_DirtyProjectionMatrix flag
  {
    int projType = *(_DWORD *)(a1 + 0x478);  // projection type

    if ( projType == 2 )  // Physical camera
    {
      Camera::CalculateProjectionMatrixFromPhysicalProperties(
          a1 + 0x7C,                    // output: cached Matrix4x4f
          *(float*)(a1 + 52),
          a1 + 36,
          {*(float*)(a1+44), *(float*)(a1+48)},
          *(float*)(a1 + 0x3C8),        // nearClip
          *(float*)(a1 + 0x3CC),        // farClip
          *(float*)(a1 + 0x454),        // aspect
          *(int*)(a1 + 56));
    }
    else if ( projType == 1 )  // Perspective or Orthographic
    {
      if ( *(_BYTE *)(a1 + 0x487) )  // isOrthographic flag
      {
        Matrix4x4f::SetOrtho(
            a1 + 0x7C,                              // output matrix
            -(*(float*)(a1+0x3C4) * *(float*)(a1+0x454)),  // -orthoSize * aspect
              *(float*)(a1+0x3C4) * *(float*)(a1+0x454),   //  orthoSize * aspect
            -*(float*)(a1+0x3C4),                   // -orthoSize
             *(float*)(a1+0x3C4),                   //  orthoSize
             *(float*)(a1+0x3C8),                   // nearClip
             *(float*)(a1+0x3CC));                  // farClip
      }
      else  // Perspective
      {
        float fov = Camera::GetFov(a1);
        Matrix4x4f::SetPerspective(
            a1 + 0x7C,              // output matrix
            fov,
            *(float*)(a1 + 0x454),  // aspect ratio
            *(float*)(a1 + 0x3C8),  // nearClip
            *(float*)(a1 + 0x3CC)); // farClip
      }
    }

    *(_BYTE *)(a1 + 0x474) = 0;  // clear dirty flag
  }

  return a1 + 0x7C;  // return pointer to cached matrix
}
```

---

## Pseudocode — Engine (x64)

```cpp
const Matrix4x4f *__fastcall Camera::GetProjectionMatrix(Camera *this, __int64 a2)
{
  // Early return if NOT dirty — cached matrix is still valid
  if ( !*((_BYTE *)this + 0x528) )       // m_DirtyProjectionMatrix flag
    return (Matrix4x4f*)((char*)this + 0xD0);  // return cached matrix directly

  int projType = *((_DWORD *)this + 331); // = this + 0x52C

  if ( projType == 1 )  // Perspective or Orthographic
  {
    float farClip  = *((float*)this + 288);  // this + 0x480
    float nearClip = *((float*)this + 287);  // this + 0x47C
    float aspect   = *((float*)this + 322);  // this + 0x508

    if ( *((_BYTE*)this + 0x53B) )  // isOrthographic
    {
      Matrix4x4f::SetOrtho(
          (Matrix4x4f*)((char*)this + 0xD0),
          -(*((float*)this + 286) * aspect),   // -orthoSize * aspect  (this + 0x478)
           *((float*)this + 286) * aspect,
          -*((float*)this + 286),              // -orthoSize
           *((float*)this + 286),
           nearClip,
           farClip);
    }
    else  // Perspective
    {
      float fov = Camera::GetFov(this);
      Matrix4x4f::SetPerspective(
          (Matrix4x4f*)((char*)this + 0xD0),
          fov, aspect, nearClip, farClip);
    }
  }
  else if ( projType == 2 )  // Physical camera
  {
    Camera::CalculateProjectionMatrixFromPhysicalProperties(
        (float*)this + 52,             // output: this + 0xD0
        *((float*)this + 34),          // this + 0x88
        (float*)this + 30,             // this + 0x78
        {*(this+0x80), *(this+0x84)},  // sensor size
        *((float*)this + 287),         // nearClip
        *((float*)this + 288),         // farClip
        *((float*)this + 322),         // aspect
        *((_DWORD*)this + 35));        // this + 0x8C
  }

  *((_BYTE*)this + 0x528) = 0;         // clear dirty flag
  return (Matrix4x4f*)((char*)this + 0xD0);  // cached matrix
}
```

---

## Camera struct field offsets

| Field | Android offset | Windows offset | Type | Description |
|---|---|---|---|---|
| `m_ProjectionMatrix` (cached) | `+0x7C` | `+0xD0` | `Matrix4x4f` (64 bytes) | **Cached projection matrix — read this externally** |
| `m_DirtyProjectionMatrix` | `+0x474` | `+0x528` | `bool` | Dirty flag — 1=needs recompute, 0=cached is valid |
| `m_ProjectionType` | `+0x478` | `+0x52C` | `int` | 1=Perspective/Ortho, 2=Physical |
| `m_Orthographic` | `+0x487` | `+0x53B` | `bool` | True=orthographic, false=perspective |
| `m_OrthographicSize` | `+0x3C4` | `+0x478` | `float` | Half-height of orthographic view |
| `m_NearClip` | `+0x3C8` | `+0x47C` | `float` | Near clip plane |
| `m_FarClip` | `+0x3CC` | `+0x480` | `float` | Far clip plane |
| `m_Aspect` | `+0x454` | `+0x508` | `float` | Viewport aspect ratio (width/height) |

---

## Projection modes

| `m_ProjectionType` | `m_Orthographic` | Mode | Matrix function |
|---|---|---|---|
| `1` | `false` | Perspective | `Matrix4x4f::SetPerspective(fov, aspect, near, far)` |
| `1` | `true` | Orthographic | `Matrix4x4f::SetOrtho(left, right, bottom, top, near, far)` |
| `2` | — | Physical Camera | `Camera::CalculateProjectionMatrixFromPhysicalProperties(...)` |

---

## Dirty flag pattern

Unity uses a lazy-recompute pattern:
1. When camera parameters change (FOV, near/far, etc.) → set `m_DirtyProjectionMatrix = 1`
2. On next `GetProjectionMatrix()` call → recompute and cache at the fixed offset, clear flag

**External implication:** If `m_DirtyProjectionMatrix == 0`, the cached matrix at the fixed offset is guaranteed valid. In a running game, this is almost always the case since the render loop calls `GetProjectionMatrix` every frame.

---

## External read — full chain (Android)

```cpp
// 1. Get managed Camera object (from IL2CPP, game-specific)
uint32_t managed_camera = ...;

// 2. Native Camera* (managed object + 0x08)
uint32_t native_camera = ReadMem<uint32_t>(managed_camera + 0x08);

// 3. Read cached projection matrix directly
Matrix4x4f proj = ReadMem<Matrix4x4f>(native_camera + 0x7C);

// Optional: check dirty flag first
// bool dirty = ReadMem<uint8_t>(native_camera + 0x474);
// if dirty, matrix might be from previous frame (still usable for ESP)
```

---

## Functions referenced (not yet documented)

| Function | Purpose |
|---|---|
| `Camera::GetFov` | Returns vertical FOV (float, degrees or radians) — needed for perspective matrix |
| `Matrix4x4f::SetPerspective` | Builds GL-style perspective matrix from fov/aspect/near/far |
| `Matrix4x4f::SetOrtho` | Builds orthographic projection matrix |
| `Camera::CalculateProjectionMatrixFromPhysicalProperties` | Physical camera (focal length, sensor size) → projection matrix |

---

## Function renames (Android IDB)

| Original | Rename | Reason |
|---|---|---|
| `sub_2D1940` | `Camera::GetProjectionMatrix` | Confirmed by Windows symbol |
