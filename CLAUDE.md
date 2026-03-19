# unity-native-ctx — AI Agent Guide

## What is this project

Context repository and C++ code for reverse engineering Unity internals on Android.
- **Engine**: Unity 2018.4.11f1
- **Architecture**: armeabi-v7a (32-bit ARM)
- **Source**: IDB of `libunity.so`
- **Approach**: External — read-only process memory (`process_vm_readv`)

The goal is to document Unity's native internal functions (e.g. `Transform::get_position`, `Camera::get_main`) and reconstruct them in C++.

---

## Repository structure

```
context/          ← Reversed pseudocode and notes per Unity module
  Transform/
    get_position.md
    ...
  README.md       ← Documentation format conventions

include/
  unity.h         ← Reconstructed C++ structs and native function signatures

src/
  memory.h        ← External memory reading wrapper

ida/
  setup.md        ← IDA Pro configuration: local types, renames, function pointers
```

---

## How to read the context

Each file in `context/` documents **one** Unity native function:

- `## Pseudocode (IDA)` — raw pseudocode from IDA Pro
- `## Signature` — clean reconstructed C++ signature
- `## Notes` — reversing observations (registers, patterns, inlining, etc.)

---

## Agent conventions

When asked to reverse a Unity function's pseudocode:

1. Identify the Unity function name (e.g. `Transform::get_position`)
2. Analyze which structs it accesses and what it returns
3. Propose a clean C++ signature
4. Document it following the format in `context/README.md`

Always use types defined in `include/unity.h` (Vector3, Quaternion, etc.).

---

## CRITICAL: External-only objective

**The target is always EXTERNAL** — read-only memory access via `process_vm_readv` from outside the game process.

- **Never** frame practical notes around injection, hooks, or in-process code execution
- **Never** suggest "just call this function internally" as a practical approach
- If a function is only useful for injection/internal use, say so clearly and mark it as **context-only** (documented for call chain understanding, not for direct use)
- External = reading memory offsets, reconstructing values, following pointers — nothing more
