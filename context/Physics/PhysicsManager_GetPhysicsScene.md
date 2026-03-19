# PhysicsManager::GetPhysicsScene

**Signature:** `PhysicsScene* PhysicsManager::GetPhysicsScene(int sceneHandle)`

**Source (engine):** `Runtime/Dynamics/PhysicsManager.cpp`

Looks up a native `PhysicsScene` object by its managed handle (`PhysicsScene::m_Handle`).
Internally performs a key lookup in `PhysicsManager::m_SceneMap` — a `core::hash_map<int, PhysicsScene*>`.

---

## Pseudocode — Android (armeabi-v7a)

```cpp
int __fastcall PhysicsManager::GetPhysicsScene(int a1)
{
  int v3 = a1;  // scene handle — stored on stack for address-passing to find()

  // Lookup in PhysicsManager::m_SceneMap (core::hash_map at PhysicsManager + 0x3C)
  _DWORD *v1 = core_hash_map_find(
      *(int**)(dword_F76000 + 60),  // PhysicsManager::m_SceneMap
      &v3);

  // End-iterator check: if result == sentinel (not found), return NULL
  if (v1 == (_DWORD*)(**(_DWORD**)(dword_F76000 + 60)         // map->data
                        + 3 * *(_DWORD*)(*(_DWORD*)(dword_F76000 + 60) + 4)  // 3 * mask
                        + 12))                                                 // + stride
    result = 0;
  else
    result = v1[2];   // bucket.value = PhysicsScene* (at node + 0x08)

  return result;
}
```

---

## Pseudocode — Engine (x64)

```cpp
struct PhysicsScene* __fastcall PhysicsManager::GetPhysicsScene(int a1)
{
  // Assertions (editor-only, non-fatal — only log):
  if (!*(_QWORD*)(qword_144536800 + 96))
    DebugStringToFile("Cannot get a 3D physics scene because the scene map is NULL.");
  if (a1 == -1)
    DebugStringToFile("Cannot get the physics scene because the physics scene handle is invalid.");

  // Lookup in PhysicsManager::m_SceneMap (core::hash_map at PhysicsManager + 0x60)
  int v14 = a1;
  __int64 v2 = *(_QWORD*)(qword_144536800 + 96);  // PhysicsManager::m_SceneMap
  __int64 v4;
  core::hash_map<uint, PhysicsScene*>::find(v2, &v4, &v14);

  // End-iterator check
  if (*(_QWORD*)v2 + 3 * (*(unsigned int*)(v2 + 8) + 8i64) == v4)
    result = nullptr;
  else
    result = *(struct PhysicsScene**)(v4 + 16);  // bucket.value = PhysicsScene* (at node + 0x10)

  return result;
}
```

---

## core::hash_map internals

Unity uses a custom open-addressing hash map (`core::hash_map`). Hash function is `core::hash<unsigned int>`.

### Hash function (both platforms, same algorithm):

```cpp
// core::hash<unsigned int>::operator()(uint key)
uint hash = key;
uint h = (4097 * hash + 2127912214) ^ ((4097 * hash + 2127912214) >> 19) ^ 0xC761C23C;
h = 9 * ((33 * h - 369570787) ^ (16896 * h - 1395695104)) - 42973499;
h = h ^ (h >> 16) ^ 0xB55A4F09;
// bucket_index = h & mask
```

### Bucket layout

| Field | Android offset | Windows offset | Type | Description |
|---|---|---|---|---|
| `hash_tag` | `+0x00` | `+0x00` | uint32 | Hash bits (bottom 2 bits stripped). `0xFFFFFFFF` = empty/end slot |
| `key` | `+0x04` | `+0x08` | int32 | Scene handle |
| `value` | `+0x08` | `+0x10` | ptr | `PhysicsScene*` (uint32 on ARM, uint64 on x64) |

**Bucket stride:** 12 bytes (Android) / 24 bytes (Windows)

### hash_map struct layout

| Field | Android offset | Windows offset | Type | Description |
|---|---|---|---|---|
| `data` | `+0x00` | `+0x00` | ptr | Bucket array base pointer |
| `mask` | `+0x04` | `+0x08` | uint | `(capacity - 1) × stride_unit` — used for `hash & mask` to get bucket offset. `stride_unit` = 4 (ARM) / 8 (x64) |

Sentinel (end iterator) = `data + 3 × (mask + stride_unit)` = `data + capacity × stride` (one past last bucket).

### Collision resolution

Linear probing with increasing step:
```cpp
probe_offset = 4;           // Android: starts at stride_unit = 4
// Windows: starts at 8
while (slot != match && slot != empty):
    bucket_offset = (bucket_offset + probe_offset) & mask
    probe_offset  += stride_unit
```

---

## PhysicsManager::m_SceneMap location

| Platform | Offset | Access |
|---|---|---|
| armeabi-v7a | `+0x3C` | `*(int**)(PhysicsManager + 0x3C)` → hash_map ptr |
| x64 (engine) | `+0x60` | `*(_QWORD*)(PhysicsManager + 0x60)` → hash_map ptr |

---

## Native PhysicsScene struct (returned value)

Not the managed `PhysicsScene` struct (which is just `int m_Handle`). This is the C++ native object:

| Field | Android offset | Windows offset | Type | Description |
|---|---|---|---|---|
| `PxScene*` | `+0x08` | `+0x10` | ptr | PhysX scene pointer — `*(DWORD*)(v11 + 8)` (Android), `*(QWORD*)(v9 + 16)` (Windows) |

> Offsets confirmed from `sub_643F00` (Android): `v18 = *(DWORD*)(v11 + 8)` and from Windows `Raycast`: `v13 = *((_QWORD*)v9 + 2)` = `+0x10`.

---

## Scene handle conventions

| Handle | Meaning |
|---|---|
| `-1` | Invalid — engine asserts on this (Windows). Never pass. |
| `0` | Default physics scene (`Physics.defaultPhysicsScene.m_Handle`) — almost certainly 0 |
| `> 0` | Additional physics scenes (local physics, multi-scene setups — Unity 2018.3+) |

---

## Global / function renames (Android IDB)

| Original | Rename | Reason |
|---|---|---|
| `dword_F76000` | `g_PhysicsManager` | PhysicsManager singleton global — `dword_F76000 + 60` = `m_SceneMap` at offset 0x3C. Same object returned by `GetManagerFromContext(9)` |
| `_find___hash_map_IPEAX...` | `core::hash_map<int,PhysicsScene*>::find` | Confirmed by engine IDB symbol: `core::hash_map<unsigned int,void *,...>::find` |

## Global renames (Windows IDB)

| Original | Rename | Reason |
|---|---|---|
| `qword_144536800` | `g_PhysicsManager` | Windows PhysicsManager singleton; `+0x60` = `m_SceneMap` |

---

## Practical notes

### For injection-based Raycast

You do **not** need to call `GetPhysicsScene` directly. The Raycast functions call it internally:
- `sub_643F00` calls `sub_65C404(scene_handle)` internally
- `PhysicsScene_CUSTOM_Internal_Raycast_Injected` passes `*a1` (scene handle) directly

**Just pass `0` as the scene handle** — that is `Physics.defaultPhysicsScene.m_Handle`.

### Reading the default scene handle externally (optional verification)

If you want to confirm handle 0 is valid:
```
gManagerContext + 9*4                        → PhysicsManager*
PhysicsManager + 0x3C                        → hash_map*
hash_map->data + 3 × hash_fn(0) & mask       → bucket
bucket[2]                                    → PhysicsScene* (non-NULL = handle 0 is valid)
```

Or simply: read `Physics.defaultPhysicsScene.m_Handle` from the managed layer (IL2CPP field of a static property).

### Why not just look up PxScene* directly?

Even if you had the `PxScene*`, you can't use it externally — it's a PhysX internal object. You need code injection to call the PhysX vtable method. The scene handle is what you need for the injection call.
