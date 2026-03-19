# Reference implementations

Functional C++ reconstructions of Unity's Transform functions.
Used to cross-validate pseudocode documentation and struct layouts.

| File | Unity version | Platform | Source | Notes |
|---|---|---|---|---|
| `get_position_gamesdk.cpp` | 2018.4.11f1 | armeabi-v7a | project owner | Includes caching — ignore for reversing. Confirms struct offsets. |
| `get_position_glm.cpp` | 2018.4.11f1 | armeabi-v7a | third party | Personal naming convention ("TelemetryObj"). Confirms NEON constants and swizzles. |
| `get_position_uc.cpp` | 2022 | x64 Windows | [UnknownCheats — EFT thread](https://www.unknowncheats.me/forum/escape-from-tarkov/677489-transform-getposition-unity-2022-a.html) | Different offsets vs 2018. SSE constants identical. Useful for cross-version comparison. |
| `get_position_uc2.cpp` | unknown | x64 Windows | [UnknownCheats — Unity external bone thread](https://www.unknowncheats.me/forum/unity/280145-unity-external-bone-position-transform.html) | 3 implementations (C++, C++ with structs, C#). Most valuable: impl [2] has named structs (`TransformAccessReadOnly`, `TransformData`, `Matrix34`) and labeled shuffle masks (xxxx, yyyy, zwxy, etc). |
| `get_position_codm.cpp` | unknown | Android 64-bit | [UnknownCheats — Unity external world position](https://www.unknowncheats.me/forum/unity/479921-external-world-position-transform.html) | Call of Duty Mobile. **Most readable**: scalar (non-SIMD) form of the quaternion math — no SSE/NEON intrinsics. Best reference for understanding the algorithm. Shows both TMatrix variants (0x28 and 0x30). |
| `get_position_neon.cpp` | unknown | armeabi-v7a | unknown | Custom NEON helpers (`extract_vec`=vextq, `swap_qwords`=vrev64q) make swizzles human-readable. Confirms offsets 0x20 (TransformAccessReadOnly), 0x10 (nodes). **Key**: reads nodes+parentIndices as a single `uint64_t` — direct evidence of the ARM LDRD packing seen in the IDA pseudocode (low32=nodes, high32=parentIndices). |
