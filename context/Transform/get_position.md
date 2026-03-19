# Transform::get_position

**Signature:** `void get_position(Transform* self, Vector3* out)`

## Pseudocode (IDA)

```cpp
// TODO: paste IDA pseudocode here
```

## Reconstructed signature

```cpp
void Transform_get_position(Transform* self, Vector3* out);
```

## Notes

- Returns world space position
- Result is written to `out` pointer (not returned by value on armeabi-v7a)
- Internally accesses the Transform component in Unity's scene graph
