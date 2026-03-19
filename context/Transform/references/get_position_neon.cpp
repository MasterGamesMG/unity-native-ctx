// Reference implementation — UnitySDK (Android, armeabi-v7a)
// Source: unknown
//
// Notable:
//   1. Custom NEON helper functions that map 1:1 to intrinsics — makes swizzles human-readable.
//   2. Explicit uint64_t trick: reads nodes + parentIndices together in one 8-byte read.
//      Low 32 bits = nodes pointer, high 32 bits = parentIndices pointer.
//      This is what the ARM LDRD in the IDA pseudocode produces (v6 split into LODWORD/HIDWORD).
//
// Offsets (armeabi-v7a — matches our target):
//   TransformAccessReadOnly : nativeTransform + 0x20   (GicanOffMgr::Unity::GetPosA_GetRotB)
//   index                   : nativeTransform + 0x24   (0x20 + sizeof(ptr))
//   nodes + parentIndices   : hierarchy + 0x10         (GicanOffMgr::Unity::GetPosB_GetRotC)
//                             packed as uint64_t: low32=nodes, high32=parentIndices
//   TransformNode stride    : 0x30 (position +0x00, rotation +0x10, scale +0x20)
//
// __glob_floats0/1/2 are the same NEON constants as in get_position_glm.cpp:
//   __glob_floats0 = [-2, -2,  2, 0]   (v12 in IDA pseudocode)
//   __glob_floats1 = [ 2, -2, -2, 0]   (v14 in IDA pseudocode)
//   __glob_floats2 = [-2,  2, -2, 0]   (v16 in IDA pseudocode)

#include "../../UnitySDK.h"
#include "../../OffsetMgr.hpp"
#include <arm_neon.h>
#include "LinuxProcess.h"

// Helper: extract single lane from float32x4_t
float32_t vectorGetByIndex(float32x4_t c, unsigned int i)
{
    return c[i];
}

// Equivalent to vmulq_f32(a, vmovq_n_f32(b[lane]))
// Multiplies all 4 lanes of 'a' by a single scalar from lane 'lane' of 'b'
float32x4_t vmult_lane(float32x4_t a, float32x4_t b, int lane)
{
    float32x4_t tmp = vmovq_n_f32(vectorGetByIndex(b, lane));
    return vmulq_f32(a, tmp);
}

float32x4_t vmult_vec(float32x4_t a, float32x4_t b)
{
    return vmulq_f32(a, b);
}

// Equivalent to vextq_f32(a, b, startIndex)
// Concatenates a and b, then extracts 4 elements starting at 'startIndex'
float32x4_t extract_vec(float32x4_t a, float32x4_t b, int startIndex)
{
    float32x4_t __result;

    bool onFirst = true;
    for (int i = 0, j = startIndex; i < 4; i++, j++)
    {
        if (j > 3)
        {
            j = 0;
            onFirst = false;
        }

        __result[i] = onFirst ? a[j] : b[j];
    }

    return __result;
}

// Equivalent to vrev64q_f32(a)
// Swaps adjacent pairs: [a, b, c, d] -> [b, a, d, c]
float32x4_t swap_qwords(float32x4_t a)
{
    float32x4_t result = a;

    for (int i = 0; i < 4; i += 2)
    {
        float tmp = result[i + 1];
        result[i + 1] = result[i];
        result[i] = tmp;
    }

    return result;
}

void UnitySDK::Transform::get_position(uintptr_t transform, glm::vec3 & out_pos)
{
    float32x4_t __final_pos;

    uintptr_t tf_unity_class = Component::get_unity_class(transform);

    if (tf_unity_class)
    {
        uintptr_t idx_0;

        // TransformAccessReadOnly at nativeTransform + 0x20
        uintptr_t v4 = tf_unity_class + GicanOffMgr::Unity::GetPosA_GetRotB; // 0x20
        uintptr_t unk_1; g_Proc->ReadMemory(tf_unity_class + GicanOffMgr::Unity::GetPosA_GetRotB, &unk_1, sizeof(uintptr_t)); // hierarchy pointer
        uintptr_t indice_start; g_Proc->ReadMemory(v4 + 4, &indice_start, sizeof(uintptr_t));                                   // index (+0x24)
        uintptr_t global_pos_index = 3 * indice_start;  // 3 floats per position (Vector3 stride in 16-byte elements) = 0x30 * index / 0x10

        // Key: reads 8 bytes at hierarchy + 0x10 as uint64_t
        //   Low  32 bits = nodes pointer         (LODWORD v6 in IDA)
        //   High 32 bits = parentIndices pointer  (HIDWORD v6 in IDA)
        // This matches the ARM LDRD instruction observed in the pseudocode.
        uint64_t global_metrics = g_Proc->ReadMemoryWrapper<uint64_t>(
            g_Proc->ReadMemoryWrapper<uintptr_t>(v4) + GicanOffMgr::Unity::GetPosB_GetRotC); // 0x10

        // nodes[index].position — stride 0x10 * (3 * index) = 0x30 * index
        if (g_Proc->ReadMemory(global_metrics + 0x10 * global_pos_index, &__final_pos, sizeof(float32x4_t)) &&
            g_Proc->ReadMemory(tf_unity_class + GicanOffMgr::Unity::GetPosA_GetRotB + 0x4, &idx_0, sizeof(uintptr_t)) &&
            global_metrics >> 32) // high32 = parentIndices pointer must be valid
        {
            // Walk hierarchy: parentIndices[i] is the parent's index; -1 means root reached
            for (int loop_count = 0,
                     i = g_Proc->ReadMemoryWrapper<int>((uintptr_t)(global_metrics >> 32) + 4 * indice_start);
                 i >= 0 && loop_count < 0xFFFF;
                 i = g_Proc->ReadMemoryWrapper<uintptr_t>((global_metrics >> 32) + 4 * i), loop_count++)
            {
                uintptr_t pos_rot_scale_addr = global_metrics + 0x30 * i;
                float32x4_t curr_child_pos;   g_Proc->ReadMemory(global_metrics + 0x30 * i,        &curr_child_pos,   sizeof(float32x4_t)); // +0x00
                float32x4_t curr_child_rot;   g_Proc->ReadMemory(global_metrics + 0x30 * i + 0x10, &curr_child_rot,   sizeof(float32x4_t)); // +0x10
                float32x4_t curr_child_scale; g_Proc->ReadMemory(global_metrics + 0x30 * i + 0x20, &curr_child_scale, sizeof(float32x4_t)); // +0x20

                // Swizzle mapping (rotation quaternion = [X, Y, Z, W]):
                //   rot_middle_swap          = vextq_f32(rot, rot, 2) = [Z, W, X, Y]  (v17 in IDA)
                //   rot_middle_swap_permuted = vrev64q_f32(ZWXY)      = [W, Z, Y, X]  (v20 in IDA)
                //   curr_child_rot_permuted  = vrev64q_f32(rot)       = [Y, X, W, Z]  (v21 in IDA)
                float32x4_t unk_vec_1_dot_rot_lane_1         = vmult_lane(__glob_floats0, curr_child_rot, 1);
                float32x4_t unk_vec_2_dot_rot_lane_0         = vmult_lane(__glob_floats1, curr_child_rot, 0);
                float32x4_t rot_middle_swap                  = extract_vec(curr_child_rot, curr_child_rot, 2); // ZWXY
                float32x4_t unk_vec_2_dot_rot_lane_2         = vmult_lane(__glob_floats1, curr_child_rot, 2);
                float32x4_t unk_vec_1_dot_rot_lane_2         = vmult_lane(__glob_floats0, curr_child_rot, 2);
                float32x4_t rot_middle_swap_permuted         = swap_qwords(rot_middle_swap);               // WZYX
                float32x4_t curr_child_rot_permuted          = swap_qwords(curr_child_rot);                // YXWZ
                float32x4_t rot_permuted_dot_unk_vec_dot_rot_lane_0 = vmulq_f32(
                    curr_child_rot_permuted,
                    vmult_lane(__glob_floats2, curr_child_rot, 0));
                float32x4_t rot_permuted_dot_unk_vec_dot_rot_lane_1 = vmulq_f32(
                    curr_child_rot_permuted,
                    vmult_lane(__glob_floats2, curr_child_rot, 1));
                float32x4_t vec_diffs_1 = vsubq_f32(
                    vmulq_f32(rot_middle_swap, unk_vec_2_dot_rot_lane_0),
                    vmulq_f32(rot_middle_swap_permuted, unk_vec_1_dot_rot_lane_1));

                // scale * accumulator
                float32x4_t _scaled_final_result = vmulq_f32(__final_pos, curr_child_scale);

                // result = parent.position + scale(result) + QuaternionRotate(parent.rotation, scale(result))
                __final_pos = vaddq_f32(
                    curr_child_pos,
                    vaddq_f32(
                        vaddq_f32(
                            _scaled_final_result,
                            vmult_lane(
                                vsubq_f32(
                                    rot_permuted_dot_unk_vec_dot_rot_lane_1,
                                    vmulq_f32(rot_middle_swap, unk_vec_2_dot_rot_lane_2)),
                                _scaled_final_result, 0)),
                        vaddq_f32(
                            vmult_lane(
                                vsubq_f32(
                                    vmulq_f32(rot_middle_swap_permuted, unk_vec_1_dot_rot_lane_2),
                                    rot_permuted_dot_unk_vec_dot_rot_lane_0),
                                _scaled_final_result,
                                1),
                            vmult_lane(vec_diffs_1, _scaled_final_result, 2))));
            }

            memcpy(&out_pos[0], &__final_pos, sizeof(float) * 3);
        }
    }
}
