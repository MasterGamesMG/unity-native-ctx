// Reference implementation — Call of Duty Mobile (Android, 64-bit)
// Source: https://www.unknowncheats.me/forum/unity/479921-external-world-position-transform.html
//
// Most valuable: scalar (non-SIMD) version of the quaternion rotation math.
// Makes the algorithm fully readable without decoding SSE/NEON intrinsics.
//
// Offsets (64-bit Android):
//   TransformAccessReadOnly : transObj + 0x38
//   index                   : transObj + 0x40  (= 0x38 + sizeof(kaddr))
//   nodes   (matrix_list)   : matrix + 0x08
//   parentIndices           : matrix + 0x10
//
// TMatrix size — two variants depending on Unity version:
//   0x28 = Vector3 position + Quaternion + Vector3 scale  (3+4+3 floats = 40 bytes)
//   0x30 = Vector4 position + Quaternion + Vector4 scale  (4+4+4 floats = 48 bytes)
// The NEON armeabi-v7a code reads float32x4_t (16 bytes) for each field → 0x30 is correct for our target.

// typedef uintptr_t kaddr;

// For sizeof(TMatrix) == 0x28
struct TMatrix_28 {
    Vector3    position;   // +0x00  12 bytes
    Quaternion rotation;   // +0x0C  16 bytes
    Vector3    scale;      // +0x1C  12 bytes
};

// For sizeof(TMatrix) == 0x30  ← matches our NEON target (armeabi-v7a)
struct TMatrix_30 {
    Vector4    position;   // +0x00  16 bytes
    Quaternion rotation;   // +0x10  16 bytes
    Vector4    scale;      // +0x20  16 bytes
};

static Vector3 Transform::getPosition(kaddr transObj)
{
    kaddr matrix = Read<kaddr>(transObj + 0x38);  // TransformHierarchy*
    kaddr index  = Read<kaddr>(transObj + 0x40);  // TransformAccessReadOnly.index

    kaddr matrix_list    = Read<kaddr>(matrix + 0x8);   // nodes
    kaddr matrix_indices = Read<kaddr>(matrix + 0x10);  // parentIndices

    // seed accumulator with local position
    Vector3 result       = Read<Vector3>(matrix_list + sizeof(TMatrix) * index);
    int transformIndex   = Read<int>(matrix_indices + sizeof(int) * index);

    while (transformIndex >= 0)
    {
        TMatrix tMatrix = Read<TMatrix>(matrix_list + sizeof(TMatrix) * transformIndex);

        float rotX = tMatrix.rotation.X;
        float rotY = tMatrix.rotation.Y;
        float rotZ = tMatrix.rotation.Z;
        float rotW = tMatrix.rotation.W;

        // apply scale to accumulator
        float scaleX = result.X * tMatrix.scale.X;
        float scaleY = result.Y * tMatrix.scale.Y;
        float scaleZ = result.Z * tMatrix.scale.Z;

        // quaternion rotation — scalar form of the NEON/SSE block
        // result = parent.position + QuaternionRotate(parent.rotation, scaledResult)
        result.X = tMatrix.position.X + scaleX
                 + (scaleX * ((rotY * rotY * -2.0f) - (rotZ * rotZ *  2.0f)))
                 + (scaleY * ((rotW * rotZ * -2.0f) - (rotY * rotX * -2.0f)))
                 + (scaleZ * ((rotZ * rotX *  2.0f) - (rotW * rotY * -2.0f)));

        result.Y = tMatrix.position.Y + scaleY
                 + (scaleX * ((rotX * rotY *  2.0f) - (rotW * rotZ * -2.0f)))
                 + (scaleY * ((rotZ * rotZ * -2.0f) - (rotX * rotX *  2.0f)))
                 + (scaleZ * ((rotW * rotX * -2.0f) - (rotZ * rotY * -2.0f)));

        result.Z = tMatrix.position.Z + scaleZ
                 + (scaleX * ((rotW * rotY * -2.0f) - (rotX * rotZ * -2.0f)))
                 + (scaleY * ((rotY * rotZ *  2.0f) - (rotW * rotX * -2.0f)))
                 + (scaleZ * ((rotX * rotX * -2.0f) - (rotY * rotY *  2.0f)));

        transformIndex = Read<int>(matrix_indices + sizeof(int) * transformIndex);
    }

    return result;
}
