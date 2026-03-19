// Reconstructed implementation of Camera::GetWorldToCameraMatrix,
// Camera::GetProjectionMatrix, and Camera::WorldToScreenPoint
// Unity 2018.4.11f1 — armeabi-v7a
//
// Status: offsets confirmed from reversing both Android and Windows IDBs
//
// Memory chain:
//   managed Camera (IL2CPP MonoObject*)
//     + 0x08  → native Camera* (m_CachedPtr)
//       + 0x3C  → m_WorldToCameraMatrix  (Matrix4x4f, 64 bytes, cached)
//       + 0x7C  → m_ProjectionMatrix     (Matrix4x4f, 64 bytes, cached)
//
// See: Camera_GetWorldToCameraMatrix.md
//      Camera_GetProjectionMatrix.md
//      Camera_get_projectionMatrix_Injected.md

namespace Unity {

    using namespace DirectX::SimpleMath;

    // -------------------------------------------------------------------------
    // Matrix4x4f — Unity's column-major 4×4 float matrix (64 bytes)
    // Same layout as DirectX::SimpleMath::Matrix (row-major storage but
    // logically transposed). Confirm with your math library's convention.
    // -------------------------------------------------------------------------
    using Matrix4x4f = Matrix;   // DirectX::SimpleMath::Matrix

    // -------------------------------------------------------------------------
    // Camera::GetWorldToCameraMatrix
    //
    // Returns the cached view matrix at native Camera* + 0x3C.
    // Unity builds this as: Scale(1,1,-1) * Transform::GetWorldToLocalMatrixNoScale
    // The -1 Z flip converts world space → camera space (Unity left-handed camera).
    //
    // Dirty flag at native Camera* + 0x474 controls lazy recomputation.
    // In a running game the flag is cleared every frame — cached value is always current.
    // -------------------------------------------------------------------------
    Matrix4x4f GetWorldToCameraMatrix(uintptr_t managed_camera) {
        // Object::GetCachedPtr — m_CachedPtr at managed object + 0x08
        const uintptr_t native = GVBXIReadObject<uint32_t>(managed_camera + 0x08);
        if (!native) return Matrix4x4f::Identity;

        // m_WorldToCameraMatrix cached at native Camera* + 0x3C
        return GVBXIReadObject<Matrix4x4f>(native + 0x3C);
    }

    // -------------------------------------------------------------------------
    // Camera::GetProjectionMatrix
    //
    // Returns the cached projection matrix at native Camera* + 0x7C.
    // Computed as one of:
    //   Perspective : Matrix4x4f::SetPerspective(fov, aspect, near, far)
    //   Orthographic: Matrix4x4f::SetOrtho(left, right, bottom, top, near, far)
    //   Physical    : Camera::CalculateProjectionMatrixFromPhysicalProperties(...)
    //
    // Dirty flag at native Camera* + 0x474.
    // -------------------------------------------------------------------------
    Matrix4x4f GetProjectionMatrix(uintptr_t managed_camera) {
        const uintptr_t native = GVBXIReadObject<uint32_t>(managed_camera + 0x08);
        if (!native) return Matrix4x4f::Identity;

        // m_ProjectionMatrix cached at native Camera* + 0x7C
        return GVBXIReadObject<Matrix4x4f>(native + 0x7C);
    }

    // -------------------------------------------------------------------------
    // Camera::WorldToScreenPoint
    //
    // Projects a world-space position to screen-space pixel coordinates (origin top-left).
    //
    // Derived from CameraProjectionCache::WorldToScreenPoint (libunity.so armeabi-v7a)
    // and Matrix4x4f::PerspectiveMultiplyPoint3 (engine x64).
    //
    // VP matrix is column-major: element[row][col] = flat[col*4 + row]
    //   clip.w = flat[3]*x + flat[7]*y + flat[11]*z + flat[15]   (row 3)
    //   clip.x = flat[0]*x + flat[4]*y + flat[8]*z  + flat[12]   (row 0)
    //   clip.y = flat[1]*x + flat[5]*y + flat[9]*z  + flat[13]   (row 1)
    //
    // Depth (signed distance along camera forward, positive = in front):
    //   depth = -dot(worldPos - view_col3_xyz, view_col2_xyz)
    //   view flat indices: col2 = [8,9,10], col3 = [12,13,14]
    //
    // Screen Y convention: top-left origin (Y=0 at top of screen).
    //   Native Unity uses bottom-left; we flip: screen_y = height - native_y
    //   Equivalent: screen_y = (1 - ndcY) * height * 0.5
    //
    // Returns {0,0,false} if:
    //   - clip.w is degenerate (|clip.w| <= 1e-7, i.e. point on/behind near plane)
    //   - worldPos is zero (fast early-out matching GameSDK reference)
    // -------------------------------------------------------------------------
    struct ScreenPoint { float x, y, depth; bool visible; };

    ScreenPoint WorldToScreenPoint(
        const Vector3&  worldPos,
        uintptr_t       managed_camera,
        int             screen_width,
        int             screen_height)
    {
        if (worldPos == Vector3::Zero)
            return { 0.0f, 0.0f, 0.0f, false };

        // Read cached view and projection matrices from native Camera*
        const Matrix4x4f view = GetWorldToCameraMatrix(managed_camera);
        const Matrix4x4f proj = GetProjectionMatrix(managed_camera);

        // Build VP = view * proj (column-major, raw float[16])
        // DirectX::SimpleMath::Matrix stores row-major but logically
        // matches Unity column-major when multiplied as view * proj.
        const Matrix4x4f vp = view * proj;

        // Treat vp as flat float[16], column-major: vp_f[col*4 + row]
        const float* vp_f   = reinterpret_cast<const float*>(&vp);
        const float* view_f = reinterpret_cast<const float*>(&view);

        const float x = worldPos.x, y = worldPos.y, z = worldPos.z;

        // Clip space W (row 3 of VP)
        const float clipW = vp_f[3]*x + vp_f[7]*y + vp_f[11]*z + vp_f[15];
        const float absW  = clipW < 0.0f ? -clipW : clipW;

        if (absW <= 0.0000001f)
            return { 0.0f, 0.0f, 0.0f, false };

        const float invW = 1.0f / clipW;

        // NDC X and Y (rows 0 and 1 of VP)
        const float ndcX = (vp_f[0]*x + vp_f[4]*y + vp_f[8]*z  + vp_f[12]) * invW;
        const float ndcY = (vp_f[1]*x + vp_f[5]*y + vp_f[9]*z  + vp_f[13]) * invW;

        // NDC → screen pixels, top-left origin
        //   screen.x =  ndcX *  width/2 + width/2
        //   screen.y = -ndcY * height/2 + height/2   (Y flipped vs native bottom-left)
        const float screenX = (screen_width  *  0.5f * ndcX) + (screen_width  * 0.5f);
        const float screenY = (screen_height * -0.5f * ndcY) + (screen_height * 0.5f);

        // Depth: signed distance along camera forward axis (positive = in front)
        // view col2 = forward (indices 8,9,10), view col3 = translation (indices 12,13,14)
        const float depth = -( (x - view_f[12]) * view_f[8]
                              + (y - view_f[13]) * view_f[9]
                              + (z - view_f[14]) * view_f[10] );

        return { screenX, screenY, depth, true };
    }

    // -------------------------------------------------------------------------
    // Optional: read individual camera properties
    // -------------------------------------------------------------------------

    float GetNearClip(uintptr_t managed_camera) {
        const uintptr_t native = GVBXIReadObject<uint32_t>(managed_camera + 0x08);
        return native ? GVBXIReadObject<float>(native + 0x3C8) : 0.f;
        // Android: native Camera* + 0x3C8 = m_NearClip
    }

    float GetFarClip(uintptr_t managed_camera) {
        const uintptr_t native = GVBXIReadObject<uint32_t>(managed_camera + 0x08);
        return native ? GVBXIReadObject<float>(native + 0x3CC) : 0.f;
        // Android: native Camera* + 0x3CC = m_FarClip
    }

    float GetAspect(uintptr_t managed_camera) {
        const uintptr_t native = GVBXIReadObject<uint32_t>(managed_camera + 0x08);
        return native ? GVBXIReadObject<float>(native + 0x454) : 0.f;
        // Android: native Camera* + 0x454 = m_Aspect
    }

    bool IsOrthographic(uintptr_t managed_camera) {
        const uintptr_t native = GVBXIReadObject<uint32_t>(managed_camera + 0x08);
        return native ? GVBXIReadObject<uint8_t>(native + 0x487) != 0 : false;
        // Android: native Camera* + 0x487 = m_Orthographic
    }

} // namespace Unity
