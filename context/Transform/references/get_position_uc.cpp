// Reference implementation — UnknownCheats forum
// Source: https://www.unknowncheats.me/forum/escape-from-tarkov/677489-transform-getposition-unity-2022-a.html
// Game: Escape from Tarkov
// Unity version: 2022 (x64 Windows) — offsets differ from 2018.4.11f1
//
// Key offset differences vs Unity 2018.4.11f1 (armeabi-v7a):
//   m_CachedPtr          : transform + 0x10   (2018: + 0x08)
//   TransformAccessReadOnly (some_ptr) : transform_internal + 0x28   (2018: + 0x20)
//   index                : transform_internal + 0x38   (0x30 + sizeof(uint64_t))  (2018: + 0x24)
//   nodes (relation_array)          : some_ptr + 0x18   (2018: + 0x10)
//   parentIndices (dependency_index_array) : some_ptr + 0x20   (2018: + 0x14)
//
// SSE constants confirmed identical to engine IDB and GLM reference:
//   xmmword_1410D1340 = { -2, 2, -2, 0 }  = __glob_floats2_glm = NEON v16
//   xmmword_1410D1350 = {  2,-2, -2, 0 }  = __glob_floats1_glm = NEON v14
//   xmmword_1410D1360 = { -2,-2,  2, 0 }  = __glob_floats0_glm = NEON v12

vec3_t GetPosition(uint64_t transform)
{
    // Object::GetCachedPtr — m_CachedPtr at +0x10 in Unity 2022 (vs +0x08 in 2018)
    auto transform_internal = g_process.read<uint64_t>(transform + 0x10);
    if (!transform_internal)
        return vec3_t();

    // TransformAccessReadOnly embedded at +0x28 within native Transform (vs +0x20 in 2018)
    auto some_ptr = g_process.read<uint64_t>(transform_internal + 0x28);            // TransformHierarchy*
    auto index    = g_process.read<int32_t>(transform_internal + 0x30 + sizeof(uint64_t)); // index (+0x38)
    if (!some_ptr)
        return vec3_t();

    auto relation_array      = g_process.read<uint64_t>(some_ptr + 0x18);  // nodes          (vs +0x10 in 2018)
    if (!relation_array)
        return vec3_t();

    auto dependency_index_array = g_process.read<uint64_t>(some_ptr + 0x20); // parentIndices  (vs +0x14 in 2018)
    if (!dependency_index_array)
        return vec3_t();

    __m128i temp_0;
    __m128 xmmword_1410D1340 = { -2.f,  2.f, -2.f, 0.f };  // __glob_floats2_glm / NEON v16
    __m128 xmmword_1410D1350 = {  2.f, -2.f, -2.f, 0.f };  // __glob_floats1_glm / NEON v14
    __m128 xmmword_1410D1360 = { -2.f, -2.f,  2.f, 0.f };  // __glob_floats0_glm / NEON v12
    __m128 temp_1;
    __m128 temp_2;

    // seed accumulator with local position — nodes[index] (stride = 48 = 0x30)
    auto temp_main      = g_process.read<__m128>(relation_array + index * 48);
    auto dependency_index = g_process.read<int32_t>(dependency_index_array + index);

    while (dependency_index >= 0)
    {
        auto relation_index = 6 * dependency_index;  // stride: 6 * 8 = 48 = 0x30 bytes per TransformNode

        temp_0 = g_process.read<__m128i>(relation_array + 8 * relation_index + 16); // node.rotation  (+0x10)
        temp_1 = g_process.read<__m128>(relation_array + 8 * relation_index + 32);  // node.scale     (+0x20)
        temp_2 = g_process.read<__m128>(relation_array + 8 * relation_index);       // node.position  (+0x00)

        __m128 v10 = _mm_mul_ps(temp_1, temp_main);  // scale * accumulator

        // quaternion shuffles — same as engine IDB SSE block
        __m128 v11 = _mm_castsi128_ps(_mm_shuffle_epi32(temp_0,  85));  // rot.yyyy
        __m128 v12 = _mm_castsi128_ps(_mm_shuffle_epi32(temp_0, 170));  // rot.zzzz
        __m128 v13 = _mm_castsi128_ps(_mm_shuffle_epi32(temp_0, 170));  // rot.zzzz
        __m128 v14 = _mm_castsi128_ps(_mm_shuffle_epi32(temp_0,   0));  // rot.xxxx
        __m128 v15 = _mm_castsi128_ps(_mm_shuffle_epi32(temp_0,   0));  // rot.xxxx
        __m128 v16 = _mm_castsi128_ps(_mm_shuffle_epi32(temp_0,  85));  // rot.yyyy

        // quaternion rotation applied to scaled accumulator + parent position
        __m128 v17 = _mm_add_ps(
            _mm_add_ps(
                _mm_add_ps(
                    _mm_mul_ps(
                        _mm_sub_ps(
                            _mm_mul_ps(_mm_mul_ps(v11, xmmword_1410D1350), v13),
                            _mm_mul_ps(_mm_mul_ps(v12, xmmword_1410D1360), v14)),
                        _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(v10), 0))),    // * scaled.x
                    _mm_mul_ps(
                        _mm_sub_ps(
                            _mm_mul_ps(_mm_mul_ps(v15, xmmword_1410D1360), v14),
                            _mm_mul_ps(_mm_mul_ps(v11, xmmword_1410D1340), v16)),
                        _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(v10), 85)))),  // * scaled.y
                _mm_add_ps(
                    _mm_mul_ps(
                        _mm_sub_ps(
                            _mm_mul_ps(_mm_mul_ps(v12, xmmword_1410D1340), v16),
                            _mm_mul_ps(_mm_mul_ps(v15, xmmword_1410D1350), v13)),
                        _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(v10), 170))),  // * scaled.z
                    v10)),
            temp_2);  // + node.position

        temp_main = v17;
        dependency_index = g_process.read<int32_t>(dependency_index_array + dependency_index); // next parent
    }

    return *reinterpret_cast<vec3_t*>(&temp_main);
}
