# CameraProjectionCache::WorldToScreenPoint

**Source (engine):** `Runtime/Camera/CameraProjectionCache.cpp`

Projects a world-space position to screen-space pixel coordinates using pre-cached VP matrix and viewport data.

---

## Pseudocode — Android (armeabi-v7a)

```cpp
// a1 = float* worldPos   (Vector3f: x, y, z)
// a2 = float* viewData   (sub-region of CameraProjectionCache — depth fields)
// a3 = float* vpMatrix   (VP matrix, column-major float[16])
// a4 = int*   viewport   ({x_offset, y_offset, width, height})
// a5 = float* output     ({screen.x, screen.y, depth})
// return: 1 if visible, 0 if behind camera

int __fastcall CameraProjectionCache::WorldToScreenPoint(
    float *a1, float *a2, float *a3, int *a4, float *a5)
{
  float x = a1[0], y = a1[1], z = a1[2];

  // clip.w = row3 of VP * vec4(world, 1)  — column-major: a3[col*4+3]
  float clipW = a3[3]*x + a3[7]*y + a3[11]*z + a3[15];
  float absW  = clipW < 0.0f ? -clipW : clipW;

  if ( absW <= 0.0000001f )          // degenerate or behind camera
  {
    *a5 = a5[1] = a5[2] = 0.0f;
    return 0;
  }

  float invW = 1.0f / clipW;

  // screen.y: clip.y = row1 of VP → ndc.y → screen
  float ndcY   = invW * (a3[1]*x + a3[5]*y + a3[9]*z + a3[13]);
  float screenY = ((ndcY + 1.0f) * (float)a4[3]) * 0.5f + (float)a4[1];

  // screen.x: clip.x = row0 of VP → ndc.x → screen
  float ndcX   = invW * (a3[0]*x + a3[4]*y + a3[8]*z + a3[12]);
  float screenX = ((ndcX + 1.0f) * (float)a4[2]) * 0.5f + (float)a4[0];

  // depth: signed distance along camera forward axis
  // a2[8..10] = camera forward (col2 of view matrix, rows 0-2)
  // a2[12..14] = translation ref (col3 of view matrix, rows 0-2)
  float depth = -((x - a2[12]) * a2[8]
               +  (y - a2[13]) * a2[9]
               +  (z - a2[14]) * a2[10]);

  *a5     = screenX;
  a5[1]   = screenY;
  a5[2]   = depth;
  return 1;
}
```

---

## Pseudocode — Engine (x64)

### CameraProjectionCache::WorldToScreenPoint
```cpp
// a1 = CameraProjectionCache*  (this)
// a2 = float*   output         ({screen.x, screen.y, depth})
// a3 = Vector3f* worldPos      (x, y, z)
// a4 = char*    visible_out    (optional — written 1/0; may be NULL)

float *__fastcall CameraProjectionCache::WorldToScreenPoint(
    __int64 a1, float *a2, __int64 a3, char *a4)
{
  // Load viewport: a1+16 → {x(int), y(int), width(int), height(int)}
  __int128 v14 = *(__int128*)(a1 + 16);
  // (int)v14     = viewport.x    (a1+16)
  // SDWORD1(v14) = viewport.y    (a1+20)
  // SDWORD2(v14) = viewport.width  (a1+24)
  // SHIDWORD(v14)= viewport.height (a1+28)

  Vector3f ndc;
  // VP matrix at a1+96 (0x60), world pos = a3
  if ( Matrix4x4f::PerspectiveMultiplyPoint3((Matrix4x4f*)(a1 + 96), (Vector3f*)a3, &ndc) )
  {
    // depth: negate dot((worldPos - translation), forward)
    // forward at a1+64/68/72, translation at a1+80/84/88
    float depth = -((*(float*)(a3+4) - *(float*)(a1+84)) * (-*(float*)(a1+68))
                 +  (*(float*)a3     - *(float*)(a1+80)) * (-*(float*)(a1+64))
                 +  (*(float*)(a3+8) - *(float*)(a1+88)) * (-*(float*)(a1+72)));
    // note: XOR with _xmm = sign flip (negate)

    float screenY = ((ndc.y + 1.0f) * (float)SHIDWORD(v14)) * 0.5f + (float)SDWORD1(v14);
    float screenX = ((ndc.x + 1.0f) * (float)SDWORD2(v14))  * 0.5f + (float)(int)v14;

    a2[0] = screenX;  a2[1] = screenY;  a2[2] = depth;
    if (a4) *a4 = 1;
  }
  else
  {
    a2[0] = a2[1] = a2[2] = 0.0f;
    if (a4) *a4 = 0;
  }
  return a2;
}
```

### Matrix4x4f::PerspectiveMultiplyPoint3
```cpp
// this = VP matrix (column-major float[16])
// a2   = Vector3f* input world position
// a3   = Vector3f* output NDC position (ndc.x, ndc.y, ndc.z)
// return: true if visible (|clip.w| > epsilon), false if degenerate

bool __fastcall Matrix4x4f::PerspectiveMultiplyPoint3(
    Matrix4x4f *this, const Vector3f *a2, Vector3f *a3)
{
  float x = a2[0], y = a2[1], z = a2[2];

  // clip.x = row0 of M * vec4(world, 1)  [column-major: this[col*4+0]]
  float clipX = x*this[0] + y*this[4] + z*this[8]  + this[12];

  // clip.w = row3 of M * vec4(world, 1)  [this[col*4+3]]
  float clipW = x*this[3] + y*this[7] + z*this[11] + this[15];

  // clip.z = row2  [this[col*4+2]]
  float clipZ = x*this[2] + y*this[6] + z*this[10] + this[14];

  if ( fabsf(clipW) <= 0.0000001f )
  {
    *a3 = {0, 0, 0};
    return false;
  }

  float invW = 1.0f / clipW;

  // clip.y = row1  [this[col*4+1]]
  float ndcY = (x*this[1] + y*this[5] + z*this[9] + this[13]) * invW;

  a3->x = clipX * invW;   // ndc.x
  a3->y = ndcY;            // ndc.y
  a3->z = clipZ * invW;   // ndc.z (depth normalized)
  return true;
}
```

---

## CameraProjectionCache struct layout

Derived from Windows offsets (`a1` = `CameraProjectionCache*`):

| Offset | Type | Field | Description |
|---|---|---|---|
| `+0x10` | `int` | `viewport.x` | Screen X offset |
| `+0x14` | `int` | `viewport.y` | Screen Y offset |
| `+0x18` | `int` | `viewport.width` | Viewport width (pixels) |
| `+0x1C` | `int` | `viewport.height` | Viewport height (pixels) |
| `+0x40` | `float` | `forward.x` | Camera forward X (col2,row0 of view matrix) |
| `+0x44` | `float` | `forward.y` | Camera forward Y (col2,row1) |
| `+0x48` | `float` | `forward.z` | Camera forward Z (col2,row2) |
| `+0x50` | `float` | `translation.x` | View translation X (col3,row0) |
| `+0x54` | `float` | `translation.y` | View translation Y (col3,row1) |
| `+0x58` | `float` | `translation.z` | View translation Z (col3,row2) |
| `+0x60` | `Matrix4x4f` | `vpMatrix` | Full VP matrix (64 bytes, column-major) |

> Android `a2[8..10]` = CameraProjectionCache `+0x40..+0x48` (forward)
> Android `a2[12..14]` = CameraProjectionCache `+0x50..+0x58` (translation)
> Android `a3[0..15]`  = CameraProjectionCache `+0x60` (VP matrix)
> Android `a4[0..3]`   = CameraProjectionCache `+0x10` (viewport)

---

## VP matrix indexing — column-major confirmed

```
Column-major storage: element[row][col] = flat_array[col*4 + row]

flat[0]  flat[1]  flat[2]  flat[3]    ← col 0 (row 0..3)
flat[4]  flat[5]  flat[6]  flat[7]    ← col 1
flat[8]  flat[9]  flat[10] flat[11]   ← col 2
flat[12] flat[13] flat[14] flat[15]   ← col 3

clip.w = flat[3]*x + flat[7]*y + flat[11]*z + flat[15]  (row 3)
clip.x = flat[0]*x + flat[4]*y + flat[8]*z  + flat[12]  (row 0)
clip.y = flat[1]*x + flat[5]*y + flat[9]*z  + flat[13]  (row 1)
```

---

## Screen Y convention

| Formula | Origin | Used by |
|---|---|---|
| `(ndcY + 1) * height * 0.5 + offset_y` | Bottom-left (Y=0 bottom) | Native Unity / this function |
| `(1 - ndcY) * height * 0.5 + offset_y` | Top-left (Y=0 top) | GameSDK reference / screen rendering |

To convert: `screen_y_topleft = height - screen_y_bottomleft`

---

## Depth formula

```cpp
depth = -dot(worldPos - translation, forward)
      = -((x - view[col3.x])*view[col2.x]
        + (y - view[col3.y])*view[col2.y]
        + (z - view[col3.z])*view[col2.z])
```

**Note:** `translation` here is the col3 of the worldToCameraMatrix, NOT the camera world position. The depth is a signed projection onto the camera's forward axis in a transformed space. Positive = in front of camera.
