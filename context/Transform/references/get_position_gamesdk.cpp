// Reference implementation — GameSDK style
// External memory read approach using GVBXIReadObject/GVBXIReadPtr
// Includes caching system (ignore for reversing purposes)
// Source: project owner

#include <GameSDK/Functions/Unity/Transform.h>
#include <GameSDK/Offsets/FF/Offsets.h>
#include <GVBXI/GVBXI.h>
#include <OMHTR/SDK.h>

namespace GameSDK {
    namespace Functions {
        namespace Unity {

            struct Matrix {
                Vector4 position; // +0x00
                Quaternion rotation; // +0x10
                Vector4 scale; // +0x20
                // total: 0x30 bytes
            };

            struct TransformIndice {
            private:
                uint64_t valid;   // TransformHierarchy* at nativeTransform + 0x20
                int start;        // index at nativeTransform + 0x24

            public:
                TransformIndice(uint64_t obj) : valid(0), start(0) {
                    if (obj != 0) {
                        valid = GVBXIReadPtr(obj + 0x20);
                        start = GVBXIReadObject<int>(obj + 0x24);
                    }
                }

                uint64_t getValid() const { return valid; }
                int getStart() const { return start; }

                // valid + 0x10 = nodes (TransformNode*)
                uint64_t getMatrixlist() const {
                    if (!valid) return 0;
                    return GVBXIReadPtr(valid + 0x10);
                }

                // valid + 0x14 = parentIndices (int*)
                uint64_t getMatrixWord() const {
                    if (!valid) return 0;
                    return GVBXIReadPtr(valid + 0x14);
                }

                Vector3 startPostion(uint64_t obj) const {
                    if (!obj) return Vector3::Zero;
                    return GVBXIReadObject<Vector3>(obj + sizeof(Matrix) * start);
                }

                Quaternion startRotation(uint64_t obj) const {
                    if (!obj) return Quaternion(0, 0, 0, 0);
                    return GVBXIReadObject<Quaternion>(obj + sizeof(Matrix) * start + sizeof(Quaternion));
                }

                Vector3 startScale(uint64_t obj) const {
                    if (!obj) return Vector3::Zero;
                    return GVBXIReadObject<Vector3>(obj + sizeof(Matrix) * start + 2 * sizeof(Vector4));
                }
            };

            Transform::Transform(uint64_t transform) : m_pRefObj(0) {
                if (transform != 0) {
                    // Object::GetCachedPtr — reads m_CachedPtr at managed object + 0x08
                    m_pRefObj = GVBXIReadPtr(transform + 0x8);
                }
            }

            Vector3 Transform::get_position() const {
                if (!IsValid()) return Vector3::Zero;
                if (m_bPositionCached) return m_CachedPosition;

                TransformIndice indice(m_pRefObj);
                uint64_t matrix_list = indice.getMatrixlist();   // nodes
                uint64_t matrix_word = indice.getMatrixWord();   // parentIndices
                if (!matrix_list || !matrix_word) return get_localPosition();

                Vector3 position = get_localPosition();
                int index = GVBXIReadObject<int>(matrix_word + sizeof(int) * indice.getStart());
                int count = 0;

                // walk hierarchy upward — same loop as CalculateGlobalPosition
                while (index >= 0 && count < 8096) {
                    Matrix matrix = GVBXIReadObject<Matrix>(matrix_list + sizeof(Matrix) * index);
                    matrix.CalculateGlobalPosition(position);
                    index = GVBXIReadObject<int>(matrix_word + sizeof(int) * index);
                    ++count;
                }

                m_CachedPosition = position;
                m_bPositionCached = true;
                return position;
            }

        }
    }
}
