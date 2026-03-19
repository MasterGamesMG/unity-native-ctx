// Reference implementations — UnknownCheats forum (multiple authors)
// Source: https://www.unknowncheats.me/forum/unity/280145-unity-external-bone-position-transform.html
// Contains 3 implementations from the thread (C++, C++ with structs, C#)
//
// Offset comparison vs other references:
//   m_CachedPtr          : transform + 0x10
//   TransformAccessReadOnly : transform_internal + 0x38   (vs +0x28 in uc.cpp 2022, +0x20 in 2018)
//   index                : transform_internal + 0x40   (0x38 + sizeof(uint64_t))
//   nodes                : some_ptr + 0x08   (vs +0x18 in uc.cpp, +0x10 in 2018)
//   parentIndices        : some_ptr + 0x10   (vs +0x20 in uc.cpp, +0x14 in 2018)

// =============================================================================
// [1] EFTData::GetPosition — C++ external read
// =============================================================================

Vector EFTData::GetPosition(uint64_t transform)
{
    auto transform_internal = this->process.ReadMemory<uint64_t>(transform + 0x10); // m_CachedPtr
    if (!transform_internal)
        return Vector();

    auto some_ptr = this->process.ReadMemory<uint64_t>(transform_internal + 0x38);               // TransformHierarchy*
    auto index    = this->process.ReadMemory<int32_t>(transform_internal + 0x38 + sizeof(uint64_t)); // index (+0x40)
    if (!some_ptr)
        return Vector();

    auto relation_array          = this->process.ReadMemory<uint64_t>(some_ptr + 0x8);  // nodes
    if (!relation_array)
        return Vector();

    auto dependency_index_array  = this->process.ReadMemory<uint64_t>(some_ptr + 0x10); // parentIndices
    if (!dependency_index_array)
        return Vector();

    __m128i temp_0;
    __m128 xmmword_1410D1340 = { -2.f,  2.f, -2.f, 0.f };  // __glob_floats2_glm / NEON v16
    __m128 xmmword_1410D1350 = {  2.f, -2.f, -2.f, 0.f };  // __glob_floats1_glm / NEON v14
    __m128 xmmword_1410D1360 = { -2.f, -2.f,  2.f, 0.f };  // __glob_floats0_glm / NEON v12
    __m128 temp_1;
    __m128 temp_2;

    auto temp_main        = this->process.ReadMemory<__m128>(relation_array + index * 48);        // nodes[index].position
    auto dependency_index = this->process.ReadMemory<int32_t>(dependency_index_array + index);    // parentIndices[index]

    while (dependency_index >= 0)
    {
        auto relation_index = 6 * dependency_index; // stride: 6 * 8 = 0x30 per TransformNode

        temp_0 = this->process.ReadMemory<__m128i>(relation_array + 8 * relation_index + 16); // node.rotation  (+0x10)
        temp_1 = this->process.ReadMemory<__m128>(relation_array + 8 * relation_index + 32);  // node.scale     (+0x20)
        temp_2 = this->process.ReadMemory<__m128>(relation_array + 8 * relation_index);       // node.position  (+0x00)

        __m128 v10 = _mm_mul_ps(temp_1, temp_main); // scale * accumulator

        // shuffle masks decoded (see impl [2] for labels):
        __m128 v11 = _mm_castsi128_ps(_mm_shuffle_epi32(temp_0,    0)); // xxxx
        __m128 v12 = _mm_castsi128_ps(_mm_shuffle_epi32(temp_0,   85)); // yyyy
        __m128 v13 = _mm_castsi128_ps(_mm_shuffle_epi32(temp_0, -114)); // zwxy (0x8E)
        __m128 v14 = _mm_castsi128_ps(_mm_shuffle_epi32(temp_0,  -37)); // wzyw (0xDB)
        __m128 v15 = _mm_castsi128_ps(_mm_shuffle_epi32(temp_0,  -86)); // zzzz (0xAA)
        __m128 v16 = _mm_castsi128_ps(_mm_shuffle_epi32(temp_0,  113)); // yxwy (0x71)

        __m128 v17 = _mm_add_ps(
            _mm_add_ps(
                _mm_add_ps(
                    _mm_mul_ps(
                        _mm_sub_ps(
                            _mm_mul_ps(_mm_mul_ps(v11, xmmword_1410D1350), v13),
                            _mm_mul_ps(_mm_mul_ps(v12, xmmword_1410D1360), v14)),
                        _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(v10), -86))), // * scaled.z (0xAA)
                    _mm_mul_ps(
                        _mm_sub_ps(
                            _mm_mul_ps(_mm_mul_ps(v15, xmmword_1410D1360), v14),
                            _mm_mul_ps(_mm_mul_ps(v11, xmmword_1410D1340), v16)),
                        _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(v10), 85)))), // * scaled.y
                _mm_add_ps(
                    _mm_mul_ps(
                        _mm_sub_ps(
                            _mm_mul_ps(_mm_mul_ps(v12, xmmword_1410D1340), v16),
                            _mm_mul_ps(_mm_mul_ps(v15, xmmword_1410D1350), v13)),
                        _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(v10), 0))),   // * scaled.x
                    v10)),
            temp_2); // + node.position

        temp_main = v17;
        dependency_index = this->process.ReadMemory<int32_t>(dependency_index_array + dependency_index);
    }

    return *reinterpret_cast<Vector*>(&temp_main);
}

// =============================================================================
// [2] Transform::GetPosition — C++ with struct definitions
// Most complete version: includes named structs and labeled shuffle masks
// =============================================================================

struct TransformAccessReadOnly
{
    ULONGLONG pTransformData; // TransformHierarchy*
    int       index;
};

struct TransformData  // = TransformHierarchy
{
    ULONGLONG pTransformArray;   // nodes          (at +0x08 from TransformHierarchy)
    ULONGLONG pTransformIndices; // parentIndices  (at +0x10 from TransformHierarchy)
};

struct Matrix34  // = TransformNode (0x30 bytes)
{
    Vector4 vec0; // +0x00  position
    Vector4 vec1; // +0x10  rotation (Quaternion)
    Vector4 vec2; // +0x20  scale
};

struct Transform
{
    Vector3 GetPosition()
    {
        __m128 result;

        const __m128 mulVec0 = { -2.000,  2.000, -2.000, 0.000 }; // __glob_floats2_glm / NEON v16
        const __m128 mulVec1 = {  2.000, -2.000, -2.000, 0.000 }; // __glob_floats1_glm / NEON v14
        const __m128 mulVec2 = { -2.000, -2.000,  2.000, 0.000 }; // __glob_floats0_glm / NEON v12

        // TransformAccessReadOnly embedded at this + 0x38
        TransformAccessReadOnly pTransformAccessReadOnly =
            Driver.ReadMem<TransformAccessReadOnly>(gTargetProcessID, (ULONGLONG)this + 0x38);

        // TransformHierarchy at pTransformData + 0x8
        TransformData transformData =
            Driver.ReadMem<TransformData>(gTargetProcessID, pTransformAccessReadOnly.pTransformData + 0x8);

        // bulk read to avoid per-iteration RPM calls
        PVOID pMatriciesBuf = malloc(sizeof(Matrix34) * pTransformAccessReadOnly.index + sizeof(Matrix34));
        PVOID pIndicesBuf   = malloc(sizeof(int)      * pTransformAccessReadOnly.index + sizeof(int));

        if (pMatriciesBuf && pIndicesBuf)
        {
            Driver.ReadMem(gTargetProcessID, transformData.pTransformArray,   pMatriciesBuf, sizeof(Matrix34) * pTransformAccessReadOnly.index + sizeof(Matrix34));
            Driver.ReadMem(gTargetProcessID, transformData.pTransformIndices, pIndicesBuf,   sizeof(int)      * pTransformAccessReadOnly.index + sizeof(int));

            result = *(__m128*)((ULONGLONG)pMatriciesBuf + 0x30 * pTransformAccessReadOnly.index); // nodes[index].position
            int transformIndex = *(int*)((ULONGLONG)pIndicesBuf + 0x4 * pTransformAccessReadOnly.index);

            while (transformIndex >= 0)
            {
                Matrix34 matrix34 = *(Matrix34*)((ULONGLONG)pMatriciesBuf + 0x30 * transformIndex);

                // shuffle masks with quaternion component labels
                __m128 xxxx = _mm_castsi128_ps(_mm_shuffle_epi32(*(__m128i*)(&matrix34.vec1), 0x00)); // rot.xxxx
                __m128 yyyy = _mm_castsi128_ps(_mm_shuffle_epi32(*(__m128i*)(&matrix34.vec1), 0x55)); // rot.yyyy
                __m128 zwxy = _mm_castsi128_ps(_mm_shuffle_epi32(*(__m128i*)(&matrix34.vec1), 0x8E)); // rot.zwxy
                __m128 wzyw = _mm_castsi128_ps(_mm_shuffle_epi32(*(__m128i*)(&matrix34.vec1), 0xDB)); // rot.wzyw
                __m128 zzzz = _mm_castsi128_ps(_mm_shuffle_epi32(*(__m128i*)(&matrix34.vec1), 0xAA)); // rot.zzzz
                __m128 yxwy = _mm_castsi128_ps(_mm_shuffle_epi32(*(__m128i*)(&matrix34.vec1), 0x71)); // rot.yxwy

                __m128 tmp7 = _mm_mul_ps(*(__m128*)(&matrix34.vec2), result); // scale * accumulator

                result = _mm_add_ps(
                    _mm_add_ps(
                        _mm_add_ps(
                            _mm_mul_ps(
                                _mm_sub_ps(
                                    _mm_mul_ps(_mm_mul_ps(xxxx, mulVec1), zwxy),
                                    _mm_mul_ps(_mm_mul_ps(yyyy, mulVec2), wzyw)),
                                _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(tmp7), 0xAA))), // * scaled.z
                            _mm_mul_ps(
                                _mm_sub_ps(
                                    _mm_mul_ps(_mm_mul_ps(zzzz, mulVec2), wzyw),
                                    _mm_mul_ps(_mm_mul_ps(xxxx, mulVec0), yxwy)),
                                _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(tmp7), 0x55)))), // * scaled.y
                        _mm_add_ps(
                            _mm_mul_ps(
                                _mm_sub_ps(
                                    _mm_mul_ps(_mm_mul_ps(yyyy, mulVec0), yxwy),
                                    _mm_mul_ps(_mm_mul_ps(zzzz, mulVec1), zwxy)),
                                _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(tmp7), 0x00))), // * scaled.x
                            tmp7)),
                    *(__m128*)(&matrix34.vec0)); // + node.position

                transformIndex = *(int*)((ULONGLONG)pIndicesBuf + 0x4 * transformIndex);
            }

            free(pMatriciesBuf);
            free(pIndicesBuf);
        }

        return Vector3(result.m128_f32[0], result.m128_f32[1], result.m128_f32[2]);
    }
};

// =============================================================================
// [3] GetBonePosition — C# (unsafe) port of the same algorithm
// =============================================================================

// struct TransformAccessReadOnly { IntPtr pTransformData; int index; }
// struct TransformData { IntPtr pTransformArray; IntPtr pTransformIndices; }
// struct Matrix34 { Vector4 vec0; Vector4 vec1; Vector4 vec2; } // 48 bytes = 0x30

private unsafe Vector3 GetBonePosition(IntPtr transform)
{
    IntPtr transform_internal = RPM.GetPtr(transform, new int[] { 0x10 }); // m_CachedPtr
    if (!RPM.IsValid(transform_internal.ToInt64()))
        return new Vector3();

    IntPtr pMatrix = RPM.GetPtr(transform_internal, new int[] { 0x38 });              // TransformHierarchy*
    int index      = RPM.Read<int>(RPM.GetPtr(transform_internal, new int[] { 0x38 + sizeof(UInt64) }).ToInt64()); // index (+0x40)
    if (!RPM.IsValid(pMatrix.ToInt64()))
        return new Vector3();

    IntPtr matrix_list_base = RPM.GetPtr(pMatrix, new int[] { 0x8 });   // nodes
    if (!RPM.IsValid(matrix_list_base.ToInt64()))
        return new Vector3();

    IntPtr dependency_index_table_base = RPM.GetPtr(pMatrix, new int[] { 0x10 }); // parentIndices
    if (!RPM.IsValid(dependency_index_table_base.ToInt64()))
        return new Vector3();

    // bulk read buffers
    void* pMatricesBuf = Marshal.AllocHGlobal(sizeof(Matrix34) * index + sizeof(Matrix34)).ToPointer();
    void* pIndicesBuf  = Marshal.AllocHGlobal(sizeof(int)      * index + sizeof(int)).ToPointer();

    RPM.ReadBytes(matrix_list_base.ToInt64(),              pMatricesBuf, sizeof(Matrix34) * index + sizeof(Matrix34));
    RPM.ReadBytes(dependency_index_table_base.ToInt64(),   pIndicesBuf,  sizeof(int)      * index + sizeof(int));

    Vector4f result        = *(Vector4f*)((UInt64)pMatricesBuf + 0x30 * (UInt64)index); // nodes[index].position
    int      index_relation = *(int*)((UInt64)pIndicesBuf + 0x4 * (UInt64)index);

    Vector4f xmmword_1410D1340 = new Vector4f(-2.0f,  2.0f, -2.0f, 0.0f); // mulVec0
    Vector4f xmmword_1410D1350 = new Vector4f( 2.0f, -2.0f, -2.0f, 0.0f); // mulVec1
    Vector4f xmmword_1410D1360 = new Vector4f(-2.0f, -2.0f,  2.0f, 0.0f); // mulVec2

    while (index_relation >= 0)
    {
        Matrix34 matrix34 = *(Matrix34*)((UInt64)pMatricesBuf + 0x30 * (UInt64)index_relation);

        Vector4f v10 = matrix34.vec2 * result; // scale * accumulator
        Vector4f v11 = VectorOperations.Shuffle(matrix34.vec1, (ShuffleSel)(0));    // xxxx
        Vector4f v12 = VectorOperations.Shuffle(matrix34.vec1, (ShuffleSel)(85));   // yyyy
        Vector4f v13 = VectorOperations.Shuffle(matrix34.vec1, (ShuffleSel)(-114)); // zwxy
        Vector4f v14 = VectorOperations.Shuffle(matrix34.vec1, (ShuffleSel)(-37));  // wzyw
        Vector4f v15 = VectorOperations.Shuffle(matrix34.vec1, (ShuffleSel)(-86));  // zzzz
        Vector4f v16 = VectorOperations.Shuffle(matrix34.vec1, (ShuffleSel)(113));  // yxwy

        result = (((((((v11 * xmmword_1410D1350) * v13) - ((v12 * xmmword_1410D1360) * v14)) * VectorOperations.Shuffle(v10, (ShuffleSel)(-86)))  // * scaled.z
                 + ((((v15 * xmmword_1410D1360) * v14) - ((v11 * xmmword_1410D1340) * v16)) * VectorOperations.Shuffle(v10, (ShuffleSel)(85))))    // * scaled.y
                 + (((((v12 * xmmword_1410D1340) * v16) - ((v15 * xmmword_1410D1350) * v13)) * VectorOperations.Shuffle(v10, (ShuffleSel)(0))) + v10)) // * scaled.x
                 + matrix34.vec0); // + node.position

        index_relation = *(int*)((UInt64)pIndicesBuf + 0x4 * (UInt64)index_relation);
    }

    Marshal.FreeHGlobal(pMatricesBufPtr);
    Marshal.FreeHGlobal(pIndicesBufPtr);

    return new Vector3(result.X, result.Y, result.Z);
}
