// Reconstructed implementation of Transform::SetPosition
// Unity 2018.4.11f1 — armeabi-v7a
//
// Status: untested
//
// Environment adaptations vs original pseudocode:
//   - Math types: DirectX::SimpleMath (Vector3, Vector4, Quaternion)
//   - Memory read:  GVBXIReadObject<T> / GVBXIReadMemory
//   - Memory write: GVBXIWriteObject<T> / GVBXIWriteMemory
//   - managed_transform: IL2CPP managed UnityEngine.Transform address

namespace Unity {

    using namespace DirectX::SimpleMath;

    // ---- internal layout (armeabi-v7a) — same as get_position.cpp ----

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
    // InverseTransformPosition
    // Scalar inverse-TRS: converts one ancestor's contribution out of world space.
    // Dual of CalculateGlobalPosition — same math, conjugate quaternion + reciprocal scale.
    // Applied recursively root-first (post-order) via the call stack.
    //
    // nodes / parent_indices: pre-read buffers (same bulk read as GetPosition).
    // node_index: the ancestor node to invert through.
    // -----------------------------------------------------------------------
    static void InverseTransformPosition(
        Vector3& result,
        const std::vector<TransformNode>& nodes,
        const std::vector<int32_t>& parent_indices,
        int node_index)
    {
        // Recurse toward root first — unwind applies transforms root → this node
        const int parent = parent_indices[node_index];
        if (parent >= 0 && parent < static_cast<int>(nodes.size()))
            InverseTransformPosition(result, nodes, parent_indices, parent);

        const TransformNode& node = nodes[node_index];

        // 1. Subtract parent position
        float tx = result.x - node.position.x;
        float ty = result.y - node.position.y;
        float tz = result.z - node.position.z;

        // 2. Apply conjugate quaternion (inverse rotation for unit quaternion):
        //    conjugate = [-rx, -ry, -rz, rw]
        const float rx = -node.rotation.x, ry = -node.rotation.y;
        const float rz = -node.rotation.z, rw =  node.rotation.w;

        const float cx = tx + tx * (ry*ry*-2.f - rz*rz* 2.f)
                            + ty * (rw*rz*-2.f + ry*rx* 2.f)
                            + tz * (rz*rx* 2.f + rw*ry* 2.f);

        const float cy = ty + tx * (rx*ry* 2.f + rw*rz* 2.f)
                            + ty * (rz*rz*-2.f - rx*rx* 2.f)
                            + tz * (rw*rx*-2.f + rz*ry* 2.f);

        const float cz = tz + tx * (rw*ry*-2.f + rx*rz* 2.f)
                            + ty * (ry*rz* 2.f + rw*rx* 2.f)
                            + tz * (rx*rx*-2.f - ry*ry* 2.f);

        // 3. Divide by scale (zero-scale guard: epsilon matches Unity's ~4.7e-10)
        constexpr float kEpsilon = 4.7e-10f;
        result.x = (std::abs(node.scale.x) < kEpsilon) ? 0.f : cx / node.scale.x;
        result.y = (std::abs(node.scale.y) < kEpsilon) ? 0.f : cy / node.scale.y;
        result.z = (std::abs(node.scale.z) < kEpsilon) ? 0.f : cz / node.scale.z;
    }

    // -----------------------------------------------------------------------
    // Transform::SetPosition
    // managed_transform: IL2CPP managed UnityEngine.Transform address.
    // world_pos: desired world-space position.
    // -----------------------------------------------------------------------
    void SetPosition(uintptr_t managed_transform, const Vector3& world_pos) {
        // Object::GetCachedPtr — m_CachedPtr at +0x08 of any IL2CPP managed object
        const uintptr_t native = GVBXIReadObject<uint32_t>(managed_transform + 0x08);
        if (!native) return;

        // TransformAccessReadOnly embedded at nativeTransform + 0x20 (armeabi-v7a)
        const auto access = GVBXIReadObject<TransformAccessReadOnly>(native + 0x20);
        if (!access.hierarchy || access.index < 0) return;

        // TransformHierarchy: nodes at +0x10, parentIndices at +0x14 (armeabi-v7a)
        const uint32_t nodes_ptr   = GVBXIReadObject<uint32_t>(access.hierarchy + 0x10);
        const uint32_t indices_ptr = GVBXIReadObject<uint32_t>(access.hierarchy + 0x14);
        if (!nodes_ptr || !indices_ptr) return;

        // Bulk read — same bounds assumption as GetPosition (parents always have lower index)
        const int n = access.index + 1;
        std::vector<TransformNode> nodes(n);
        std::vector<int32_t>       parent_indices(n);

        GVBXIReadMemory(nodes_ptr,   nodes.data(),          n * sizeof(TransformNode));
        GVBXIReadMemory(indices_ptr, parent_indices.data(), n * sizeof(int32_t));

        Vector3 local = world_pos;

        // Root transforms (index == 0 or no parent): world == local, no inversion needed
        const int parent = parent_indices[access.index];
        if (parent >= 0 && parent < n)
            InverseTransformPosition(local, nodes, parent_indices, parent);

        // Write local position to nodes[index].position (xyz into Vector4, w unchanged)
        const uint32_t node_addr = nodes_ptr + access.index * sizeof(TransformNode);
        GVBXIWriteObject<float>(node_addr + offsetof(TransformNode, position) + 0x00, local.x);
        GVBXIWriteObject<float>(node_addr + offsetof(TransformNode, position) + 0x04, local.y);
        GVBXIWriteObject<float>(node_addr + offsetof(TransformNode, position) + 0x08, local.z);
    }

    // -----------------------------------------------------------------------
    // Transform::SetLocalPosition  (simplified variant)
    // Based on reference: get_position_gamesdk.cpp — set_localRotation pattern.
    //
    // Writes directly to nodes[index].position without InverseTransformPosition.
    // This sets the LOCAL position (relative to parent), not world space.
    // For root transforms (parentIndices[index] == -1), local == world, so this
    // is equivalent to SetPosition. For non-root transforms, the position written
    // is interpreted in the parent's coordinate space.
    //
    // Use this when:
    //   - You already have the local position
    //   - The transform is a root (teleporting a character's root bone)
    //   - You don't need world-space accuracy for non-root bones
    // -----------------------------------------------------------------------
    void SetLocalPosition(uintptr_t managed_transform, const Vector3& local_pos) {
        // Object::GetCachedPtr — m_CachedPtr at +0x08 of any IL2CPP managed object
        const uintptr_t native = GVBXIReadObject<uint32_t>(managed_transform + 0x08);
        if (!native) return;

        // TransformAccessReadOnly embedded at nativeTransform + 0x20 (armeabi-v7a)
        const auto access = GVBXIReadObject<TransformAccessReadOnly>(native + 0x20);
        if (!access.hierarchy || access.index < 0) return;

        // TransformHierarchy: nodes at +0x10 (armeabi-v7a)
        const uint32_t nodes_ptr = GVBXIReadObject<uint32_t>(access.hierarchy + 0x10);
        if (!nodes_ptr) return;

        // Write xyz directly to nodes[index].position (+0x00 in TransformNode)
        // Same pattern as: GVBXIWriteObject<Quaternion>(nodes + sizeof(TransformNode) * index + 0x10, rot)
        const uint32_t node_addr = nodes_ptr + access.index * sizeof(TransformNode);
        GVBXIWriteObject<float>(node_addr + 0x00, local_pos.x);
        GVBXIWriteObject<float>(node_addr + 0x04, local_pos.y);
        GVBXIWriteObject<float>(node_addr + 0x08, local_pos.z);
    }

} // namespace Unity
