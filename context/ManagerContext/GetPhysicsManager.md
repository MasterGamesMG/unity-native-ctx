# GetPhysicsManager

**Signature:** `PhysicsManager* GetPhysicsManager(void)`

## Pseudocode — Android (armeabi-v7a)

```cpp
int GetPhysicsManager()
{
  return GetManagerFromContext(9);
}
```

## Pseudocode — Engine (x64)

```cpp
struct PhysicsManager *GetPhysicsManager(void)
{
  return GetManagerFromContext(9);
}
```

## Reconstructed signature

```cpp
uint32_t GetPhysicsManager();   // armeabi-v7a: returns native PhysicsManager* as uint32_t
```

## Notes

- Trivial wrapper — just calls `GetManagerFromContext(9)` [see GetManagerFromContext.md]
- Both Android and engine versions are identical in logic; engine adds type info in the return
- Index `9` is the PhysicsManager slot in the global manager context table (`gManagerContext`)
- The returned pointer is the native `PhysicsManager` C++ object — used internally by physics icalls such as `PhysicsScene::Internal_Raycast_Injected`
- To read the PhysicsManager address externally:
  ```cpp
  const uint32_t physics_manager = GVBXIReadObject<uint32_t>(gManagerContext_addr + 9 * 4);
  ```
