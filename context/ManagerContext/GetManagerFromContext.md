# GetManagerFromContext

**Signature:** `Object* GetManagerFromContext(int index)`

**Source (engine):** `Runtime/BaseClasses/ManagerContext.cpp`

## Pseudocode — Android (armeabi-v7a)

```cpp
int __fastcall GetManagerFromContext(int a1)
{
  return dword_F57B90[a1];
}
```

## Pseudocode — Engine (x64)

```cpp
struct Object *__fastcall GetManagerFromContext(int a1)
{
  struct Object *result = GetManagerPtrFromContext(a1);
  if ( !result )
  {
    // DebugStringToFile: "GetManagerFromContext: pointer to object of manager '%s' is NULL (table index %d)"
    // Manager name looked up from: unk_1445F8500[a1]  (array of const char*)
    // Source: ManagerContext.cpp
  }
  return result;
}
```

## Reconstructed signature

```cpp
uint32_t GetManagerFromContext(int index);   // armeabi-v7a: returns uint32_t (pointer)
```

## Global renames (Android IDB)

| Original | Rename | Reason |
|---|---|---|
| `dword_F57B90` | `gManagerContext` | Global array of manager pointers indexed by manager type. `dword_F57B90[9]` = PhysicsManager. Confirmed by engine: `GetManagerPtrFromContext(a1)` reads from an equivalent table. |

## Known index values

| Index | Manager |
|---|---|
| `9` | `PhysicsManager` — confirmed by `GetPhysicsManager()` calling `GetManagerFromContext(9)` on both platforms |

Additional indices can be discovered by finding other `GetXxxManager()` wrappers that follow the same pattern.

## Notes

- Android version is a single-instruction function: load from global array at `gManagerContext + index * 4` (32-bit word per entry)
- Engine version wraps `GetManagerPtrFromContext` (further indirection) and adds a null-check with `DebugStringToFile` — error path only, irrelevant for external use
- `unk_1445F8500` (engine) is a `const char*[]` of manager names used only in the debug log; not needed externally
- To read PhysicsManager pointer externally: `GVBXIReadObject<uint32_t>(base + offsetof(gManagerContext) + 9 * 4)`
- The manager pointer returned is the native C++ object, not a managed IL2CPP wrapper
