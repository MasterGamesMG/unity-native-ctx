// Reference implementation — GLM style
// Uses GMPHOFF_SELF offsets and Object/Derref abstraction
// Source: third party (naming may differ from Unity internals)
// Note: "telemetry" naming is personal convention, not Unity's internal name

#include <glm/glm.hpp>

// quaternion rotation constants — confirmed against NEON assembly
// these are the global float4 vectors used in the rotation matrix computation
glm::vec4 __glob_floats0_glm = { -2.f, -2.f, 2.f, 0.f };   // NEON v12
glm::vec4 __glob_floats1_glm = {  2.f, -2.f, -2.f, 0.f };  // NEON v14
glm::vec4 __glob_floats2_glm = { -2.f,  2.f, -2.f, 0.f };  // NEON v16

// TransformNode (0x30 bytes) — called "TelemetryObj" in this implementation
struct TelemetryObj {
    glm::vec4 pos;    // +0x00
    glm::vec4 rot;    // +0x10  (Quaternion xyzw)
    glm::vec4 scale;  // +0x20
};

glm::vec3 Transform::GetPosition()
{
    if (!mTF)
        return glm::vec3(0.f);

    // mTF + GMPHOFF_SELF("unity.tf.telemetry") = nativeTransform + 0x20 = TransformAccessReadOnly
    Object telemetryInfo = mTF.Derref(GMPHOFF_SELF("unity.tf.telemetry"));

    // fence check — if active, bail (unlike Android which calls CompleteFenceInternal)
    if (telemetryInfo.Derref(0))
        return glm::vec3(0.f);

    // telemetryInfo + pointerSize = TransformHierarchy.parentIndices (valid + 0x14 in 32-bit)
    Object telemetryIndicesPathBase = telemetryInfo.Derref(GMPHOFF_SELF("unity.telemetryinfoobj") + telemetryInfo.mProc->getPointerSize());

    // nativeTransform + 0x24 = TransformAccessReadOnly.index
    uint32_t rootIndex = mTF.Read<uint32_t>(GMPHOFF_SELF("unity.tf.telemetry") + mTF.mProc->getPointerSize());

    uint32_t entryIndx = telemetryIndicesPathBase.Read<uint32_t>(sizeof(uint32_t) * rootIndex);
    int32_t nextIdx = entryIndx;

    // telemetryInfo + GMPHOFF("unity.telemetryinfoobj") = TransformHierarchy.nodes (valid + 0x10)
    Object telemetryObjsBase = telemetryInfo.Derref(GMPHOFF_SELF("unity.telemetryinfoobj"));

    // seed accumulator with local position (nodes[rootIndex].pos)
    glm::vec4 result = telemetryObjsBase.Read<glm::vec4>(sizeof(TelemetryObj) * rootIndex);
    int count = 0;

    // walk hierarchy upward — same loop as CalculateGlobalPosition NEON
    while (nextIdx > -1 && count++ < 8096)
    {
        TelemetryObj currTlmObj = telemetryObjsBase.Read<TelemetryObj>(nextIdx * sizeof(TelemetryObj));

        nextIdx = telemetryIndicesPathBase.Read<uint32_t>(sizeof(uint32_t) * nextIdx);  // advance parent index

        const glm::vec4& rot = currTlmObj.rot;
        result *= currTlmObj.scale;                              // scale accumulator

        // quaternion swizzles — confirmed against NEON v21, v17, v20
        glm::vec4 rotYXWZ; Maths::ReverseVec(currTlmObj.rot, rotYXWZ);  // YXWZ = NEON v21 (vrev64q of rot)
        glm::vec4 rotZWXY; Maths::ExtractVec(rot, rot, rotZWXY, 2);     // ZWXY = NEON v17 (vextq 8 bytes)
        glm::vec4 rotWZYX; Maths::ReverseVec(rotZWXY, rotWZYX);         // WZYX = NEON v20 (vrev64q of v17)

        // rotation matrix columns from quaternion — matches __glob_floats constants
        glm::vec4 comp1 = (__glob_floats2_glm * rot[1]);  // [-2,2,-2,0] * rot.y
        glm::vec4 comp2 = (__glob_floats1_glm * rot[2]);  // [2,-2,-2,0] * rot.z
        glm::vec4 comp3 = (__glob_floats2_glm * rot[0]);  // [-2,2,-2,0] * rot.x
        glm::vec4 comp4 = (__glob_floats0_glm * rot[2]);  // [-2,-2,2,0] * rot.z
        glm::vec4 comp5 = (__glob_floats1_glm * rot[0]);  // [2,-2,-2,0] * rot.x
        glm::vec4 comp6 = (__glob_floats0_glm * rot[1]);  // [-2,-2,2,0] * rot.y

        comp1 *= rotYXWZ;
        comp3 *= rotYXWZ;
        comp2 *= rotZWXY;
        comp5 *= rotZWXY;
        comp4 *= rotWZYX;
        comp6 *= rotWZYX;

        comp1 -= comp2;
        comp3 = (comp4 - comp3);
        comp6 = (comp5 - comp6);

        // apply rotation columns scaled by result.x, result.y, result.z
        comp1 *= result[0];
        comp3 *= result[1];
        comp6 *= result[2];

        // accumulate: result = parent.pos + rotated_scaled_result
        result += (comp6 + comp3 + comp1 + currTlmObj.pos);
    }

    return result;
}
