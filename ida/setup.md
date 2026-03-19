# IDA Pro Setup

Configuration and structs applied to the Android IDB (`libunity.so`, armeabi-v7a).

---

## Local Types

Added via `View → Local Types → Ins`.

```cpp
struct Vector3 {
    float x;
    float y;
    float z;
};

struct Vector2 {
    float x;
    float y;
};

struct Vector4 {
    float x;
    float y;
    float z;
    float w;
};

struct Quaternion {
    float x;
    float y;
    float z;
    float w;
};

struct Color {
    float r;
    float g;
    float b;
    float a;
};
```

> To apply to a variable or parameter: right-click → `Set type` or press `Y`.

---

## Function renames applied

| Address    | Name |
|---|---|
| `sub_33FCB0` | `Transform::GetPosition` |
| `sub_3EC10C` | `Object::GetCachedPtr` |
| `sub_3EC5C0` | `RaiseNullReferenceException` |
| `sub_C5D90`  | `CalculateGlobalPosition` |

---

## Function pointer renames

| Address       | Name |
|---|---|
| `dword_F26C00` | `il2cpp_domain_get` |
| `dword_F26D04` | `il2cpp_exception_from_name` |

---

## Renames from CalculateGlobalPosition

| Address | Name |
|---|---|
| `sub_4AA310` | `CompleteFenceInternal` |
