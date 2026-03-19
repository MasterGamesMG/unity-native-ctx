// Reconstructed implementation of Transform::GetPosition
// Unity 2018.4.11f1 — armeabi-v7a
//
// Status: tested — successfully read world-space position of a non-root bone (player head)
//
// Environment adaptations vs original pseudocode:
//   - Math types: DirectX::SimpleMath (Vector3, Vector4, Quaternion)
//   - Memory read: GVBXIReadObject<T> / GVBXIReadMemory (replaces mem::read<T>)
//   - managed_transform: IL2CPP managed UnityEngine.Transform address

namespace Unity {

    using namespace DirectX::SimpleMath;

    // ---- internal layout (armeabi-v7a) ----

    struct TransformAccessReadOnly {
        uint32_t hierarchy;  // TransformHierarchy*
        int32_t  index;
    };

    struct TransformNode {    // 0x30 bytes
        Vector4    position;  // +0x00
        Quaternion rotation;  // +0x10
        Vector4    scale;     // +0x20
    };

    // -----------------------------------------------------------------------
    // CalculateGlobalPosition
    // Scalar equivalent of Unity's NEON/SSE quaternion rotation block.
    // Applies one parent node's TRS to the world-space accumulator.
    // -----------------------------------------------------------------------
    static void CalculateGlobalPosition(Vector3& result, const TransformNode& node) {
        const float rx = node.rotation.x, ry = node.rotation.y;
        const float rz = node.rotation.z, rw = node.rotation.w;
        const float sx = result.x * node.scale.x;
        const float sy = result.y * node.scale.y;
        const float sz = result.z * node.scale.z;

        result.x = node.position.x + sx
            + sx * (ry * ry * -2.f - rz * rz *  2.f)
            + sy * (rw * rz * -2.f + ry * rx *  2.f)
            + sz * (rz * rx *  2.f + rw * ry *  2.f);

        result.y = node.position.y + sy
            + sx * (rx * ry *  2.f + rw * rz *  2.f)
            + sy * (rz * rz * -2.f - rx * rx *  2.f)
            + sz * (rw * rx * -2.f + rz * ry *  2.f);

        result.z = node.position.z + sz
            + sx * (rw * ry * -2.f + rx * rz *  2.f)
            + sy * (ry * rz *  2.f + rw * rx *  2.f)
            + sz * (rx * rx * -2.f - ry * ry *  2.f);
    }

    // -----------------------------------------------------------------------
    // Transform::GetPosition
    // managed_transform: IL2CPP managed UnityEngine.Transform address.
    // -----------------------------------------------------------------------
    Vector3 GetPosition(uintptr_t managed_transform) {
        // Object::GetCachedPtr — m_CachedPtr at +0x08 of any IL2CPP managed object
        const uintptr_t native = GVBXIReadObject<uint32_t>(managed_transform + 0x08);
        if (!native) return {};

        // TransformAccessReadOnly embedded at nativeTransform + 0x20 (armeabi-v7a)
        const auto access = GVBXIReadObject<TransformAccessReadOnly>(native + 0x20);
        if (!access.hierarchy || access.index < 0) return {};

        // TransformHierarchy: nodes at +0x10, parentIndices at +0x14 (armeabi-v7a)
        const uint32_t nodes_ptr   = GVBXIReadObject<uint32_t>(access.hierarchy + 0x10);
        const uint32_t indices_ptr = GVBXIReadObject<uint32_t>(access.hierarchy + 0x14);
        if (!nodes_ptr || !indices_ptr) return {};

        // Bulk read — Unity stores nodes in pre-order, so every ancestor
        // has index < this node's index. index+1 entries covers the full chain.
        const int n = access.index + 1;
        std::vector<TransformNode> nodes(n);
        std::vector<int32_t>       parent_indices(n);

        GVBXIReadMemory(nodes_ptr,   nodes.data(),          n * sizeof(TransformNode));
        GVBXIReadMemory(indices_ptr, parent_indices.data(), n * sizeof(int32_t));

        // Seed: local position of this transform (xyz from Vector4)
        Vector3 result = {
            nodes[access.index].position.x,
            nodes[access.index].position.y,
            nodes[access.index].position.z,
        };

        // Walk hierarchy upward, applying each ancestor's TRS
        for (int i = parent_indices[access.index]; i >= 0 && i < n; i = parent_indices[i])
            CalculateGlobalPosition(result, nodes[i]);

        return result;
    }

} // namespace Unity
