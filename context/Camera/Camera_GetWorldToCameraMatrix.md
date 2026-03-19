# Camera::GetWorldToCameraMatrix

**Icall for:** `Camera.worldToCameraMatrix` (C# property getter)
**Source (engine):** `Runtime/Camera/Camera.cpp`
**Android rename:** `Camera::GetWorldToCameraMatrix` (already named in icall wrapper)

> **Externally relevant** — the view matrix is **cached** at a fixed offset inside `Camera*`.
> Read it directly from external memory. Combined with `projectionMatrix`, enables full world→screen projection for ESP.

---

## KEY: Cached matrix offset

| Platform    | Offset          | External read |
|-------------|-----------------|---------------|
| armeabi-v7a | `Camera + 0x3C` | `ReadMem<Matrix4x4f>(native_camera + 0x3C)` |
| x64 (engine)| `Camera + 0x90` | `ReadMem<Matrix4x4f>(native_camera + 0x90)` |

---

## Pseudocode — Android (armeabi-v7a)

### Icall wrapper
```cpp
__int64 *__fastcall Camera_CUSTOM_get_worldToCameraMatrix_Injected(int a1, _QWORD *a2)
{
  // Same managed→native pattern as projectionMatrix
  if ( !a1 || !Object::GetCachedPtr(a1) )
    RaiseNullReferenceException();

  int v4 = Object::GetCachedPtr(a1);              // native Camera*
  float32x4_t *v5 = Camera::GetWorldToCameraMatrix(v4);
  return Matrix4x4f::CopyTo((__int64 *)v5, a2);   // sub_831340
}
```

### Camera::GetWorldToCameraMatrix
```cpp
float32x4_t *__fastcall Camera::GetWorldToCameraMatrix(int a1)
{
  float32x4_t *v2 = (float32x4_t *)(a1 + 0x3C);  // cached matrix at Camera+0x3C

  if ( *(_BYTE *)(a1 + 0x476) )  // m_DirtyWorldToCameraMatrix flag
  {
    // Build scale vector {1.0f, 1.0f, -1.0f}
    // 0x3F8000003F800000 = {1.0f, 1.0f} packed
    // -1082130432 = 0xBF800000 = -1.0f
    Vector3f scale = {1.0f, 1.0f, -1.0f};

    Matrix4x4f::SetScale(v2, &scale);   // sub_239FE8: sets matrix to scale(1,1,-1)

    // Get camera's Transform component via GameObject
    int gameObject = *(_DWORD *)(a1 + 0x1C);   // Camera→m_GameObject (ImmediatePtr)
    int transform = GameObject::QueryComponentByType(gameObject, &TypeContainer<Transform>::rtti);
    // sub_53B6A8(gameObject, &unk_F071A4) — same as in SyncTransforms

    Transform::GetWorldToLocalMatrixNoScale(v2_temp, transform);  // sub_C6268: fills temp matrix
    Matrix4x4f::operator*=(v2, v2_temp);         // sub_239CCC: v2 *= worldToLocalNoScale
  }

  return v2;  // pointer to cached matrix at Camera+0x3C
}
```

---

## Pseudocode — Engine (x64)

### Camera::GetWorldToCameraMatrix
```cpp
const Matrix4x4f *__fastcall Camera::GetWorldToCameraMatrix(Camera *this)
{
  // Early return if NOT dirty — cached matrix is valid
  if ( !*((_BYTE *)this + 0x52A) )          // m_DirtyWorldToCameraMatrix
    return (Matrix4x4f*)((char*)this + 0x90);  // return cached matrix

  // Build scale {1.0f, 1.0f, -1.0f} on stack
  Vector3f scale = {1.0f, 1.0f, -1.0f};
  Matrix4x4f::SetScale((Matrix4x4f*)((char*)this + 0x90), &scale);
  // → sets cached matrix to scale(1, 1, -1)

  // Get camera's Transform
  // Camera+0x68 = ImmediatePtr<Transform> (or ImmediatePtr<GameObject>)
  auto *v2 = ImmediatePtr<Transform>::operator->((char*)this + 0x68);
  Transform *v3 = GameObject::QueryComponentByType(v2, &TypeContainer<Transform>::rtti);

  // Get world-to-local matrix without scale from the camera's Transform
  Matrix4x4f tmp;
  auto v4 = Transform::GetWorldToLocalMatrixNoScale(v3, tmp);

  // Concatenate: worldToCameraMatrix = scale(1,1,-1) * worldToLocalNoScale
  Matrix4x4f::operator*=((char*)this + 0x90, v4);

  return (Matrix4x4f*)((char*)this + 0x90);  // return cached matrix
}
```

---

## Camera struct fields confirmed

| Field | Android offset | Windows offset | Type | Description |
|---|---|---|---|---|
| `m_WorldToCameraMatrix` (cached) | `+0x3C` | `+0x90` | `Matrix4x4f` (64 bytes) | **View matrix — read this externally** |
| `m_DirtyWorldToCameraMatrix` | `+0x476` | `+0x52A` | `bool` | Dirty flag (adjacent to proj dirty) |
| `m_GameObject` / ImmediatePtr | `+0x1C` | `+0x68` | ptr | Reference to camera's owning GameObject/Transform |

### Full Camera dirty flags layout
```
Android:  +0x474 = m_DirtyProjectionMatrix
          +0x475 = (padding or other flag)
          +0x476 = m_DirtyWorldToCameraMatrix

Windows:  +0x528 = m_DirtyProjectionMatrix
          +0x529 = (padding or other flag)
          +0x52A = m_DirtyWorldToCameraMatrix
```

---

## Algorithm: why Scale(1, 1, -1)?

Unity uses a **left-handed coordinate system** internally but cameras look down **-Z**. The view matrix must flip Z to match this convention:

```
worldToCameraMatrix = Scale(1, 1, -1) × Transform::GetWorldToLocalMatrixNoScale
```

- `GetWorldToLocalMatrixNoScale` = inverse of the camera's Transform (rotation + translation, no scale)
- The Z flip converts from Unity world space (right-handed +Z forward) to camera space (left-handed -Z forward / OpenGL-like)

**This is why `Camera.worldToCameraMatrix ≠ Camera.transform.worldToLocalMatrix`** — the view matrix has the Z flip baked in.

---

## External use: full VP matrix for ESP

```cpp
// Android
uint32_t native_cam = ReadMem<uint32_t>(managed_camera + 0x08);

Matrix4x4f view = ReadMem<Matrix4x4f>(native_cam + 0x3C);   // worldToCameraMatrix
Matrix4x4f proj = ReadMem<Matrix4x4f>(native_cam + 0x7C);   // projectionMatrix

// ViewProjection matrix (for world→clip space)
Matrix4x4f vp = proj * view;

// Project world position to screen:
// clip = vp * vec4(world_pos, 1.0)
// ndc  = clip.xyz / clip.w
// screen.x = (ndc.x + 1) * 0.5 * screen_width
// screen.y = (1 - ndc.y) * 0.5 * screen_height  // Y flipped in Unity
```

---

## Function renames (Android IDB)

| Original     | Rename                                      | Reason |
|--------------|---------------------------------------------|--------|
| `sub_239FE8` | `Matrix4x4f::SetScale`                      | Confirmed by Windows symbol; called with Vector3f{1,1,-1} |
| `sub_C6268`  | `Transform::GetWorldToLocalMatrixNoScale`   | Confirmed by Windows symbol; fills Matrix4x4f from Transform |
| `sub_239CCC` | `Matrix4x4f::operator*=`                    | Confirmed by Windows; multiplies matrix in-place |
| `sub_53B6A8` | `GameObject::QueryComponentByType`          | Already renamed in SyncTransforms |

> `unk_F071A4` → rename to `TypeContainer<Transform>::rtti` (TypeInfo for Transform component)
