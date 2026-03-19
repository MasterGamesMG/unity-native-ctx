// Reference implementation — GameSDK style
// External memory read approach using GVBXIReadObject / GVBXIReadPtr
// Source: project owner — confirmed working
//
// Offset map (armeabi-v7a):
//   managed Camera + 0x08  → native Camera*
//   native Camera*  + 0x3C → m_WorldToCameraMatrix (Matrix4x4f, 64 bytes)
//   native Camera*  + 0x7C → m_ProjectionMatrix    (Matrix4x4f, 64 bytes)
//
// See: Camera_GetWorldToCameraMatrix.md
//      Camera_GetProjectionMatrix.md

#include <GameSDK/Functions/Unity/Camera.h>
#include <GameSDK/Offsets/FF/Offsets.h>
#include <GVBXI/GVBXI.h>
#include <OMHTR/SDK.h>

namespace GameSDK {
    namespace Functions {
        namespace Unity {

            Camera::Camera(uint64_t camera) : m_pAddress(camera), m_pRefObj(0) {
                if (camera != 0) {
                    m_pRefObj = GVBXIReadPtr(camera + 0x8);
                }
            }

            Matrix Camera::GetWorldToCameraMatrix() const {
                if (!IsValid())
                    return Matrix::Identity;

                return GVBXIReadObject<Matrix>(m_pRefObj + 0x3C);
            }

            Matrix Camera::GetProjectionMatrix() const {
                if (!IsValid())
                    return Matrix::Identity;

                return GVBXIReadObject<Matrix>(m_pRefObj + 0x7C);
            }

            Matrix Camera::GetViewProjectionMatrix() const {
                if (!IsValid()) return Matrix::Identity;

                if (m_bVPCached)
                    return m_CachedVP;

                m_CachedVP = GetWorldToCameraMatrix() * GetProjectionMatrix();
                m_bVPCached = true;
                return m_CachedVP;
            }

            Vector2 Camera::WorldToScreenPoint(const Vector3& worldPos, int width, int height) const {
                return WorldToScreenPoint(worldPos, GetViewProjectionMatrix(), width, height);
            }

            Vector2 Camera::WorldToScreenPoint(const Vector3& worldPos, const Matrix& vp, int width, int height) const {
                if (worldPos == Vector3::Zero)
                    return { 0.0f, 0.0f };

                Vector4 clipCoords = Vector4::Transform(
                    Vector4(worldPos.x, worldPos.y, worldPos.z, 1.0f), vp);

                if (clipCoords.w < 0.01f)
                    return { 0.0f, 0.0f };

                float invW = 1.0f / clipCoords.w;
                float ndcX = clipCoords.x * invW;
                float ndcY = clipCoords.y * invW;

                return {
                    (width  *  0.5f * ndcX) + (width  * 0.5f),
                    (height * -0.5f * ndcY) + (height * 0.5f)
                };
            }

        }
    }
}
