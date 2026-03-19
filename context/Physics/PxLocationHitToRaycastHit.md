# PxLocationHitToRaycastHit

**Source (engine):** `Runtime/Dynamics/PhysicsQuery.cpp`

> **Context-only** — runs inside the game process as part of the PhysX raycast pipeline. No external use.

Converts a `physx::PxLocationHit` (raw PhysX hit result) to Unity's managed `RaycastHit` struct.
Called by `Unity::PhysicsQuery::RaycastInternal` (`sub_643F00`) when a hit is detected.

**Does NOT write `m_UV`** — UV coords are written separately by the caller from a different PhysX hit field (`v61` on Android / `v23[32..36]` on Windows). See [PhysicsQuery_Raycast.md](PhysicsQuery_Raycast.md).

---

## Pseudocode — Android (armeabi-v7a)

```cpp
// sub_6424F0
int __fastcall PxLocationHitToRaycastHit(physx::PxLocationHit *a1, RaycastHit *a2)
{
  // --- Collider instance ID ---
  // actor = *(a1 + 0x04)  (PxRigidActor*)
  // *(actor + 8)          → Unity collider wrapper ptr
  // *(wrapper + 4)        → Object instance ID (direct field read, 32-bit)
  int actor = *(_DWORD *)(a1 + 4);
  int wrapper = actor ? *(_DWORD *)(actor + 8) : 0;
  int instanceID = wrapper ? *(_DWORD *)(wrapper + 4) : 0;
  *(_DWORD *)(a2 + 40) = instanceID;   // → m_Collider (+0x28)

  // --- Position (m_Point) ---
  // PxVec3 position at a1 + 0x10
  *(_QWORD *)a2            = *(_QWORD *)(a1 + 16);   // pos.xy → m_Point.xy (+0x00)
  *(_DWORD *)(a2 + 8)      = *(_DWORD *)(a1 + 24);   // pos.z  → m_Point.z  (+0x08)

  // --- Normal (m_Normal) ---
  // PxVec3 normal at a1 + 0x1C
  *(_QWORD *)(a2 + 12)     = *(_QWORD *)(a1 + 28);   // nor.xy → m_Normal.xy (+0x0C)
  *(_DWORD *)(a2 + 20)     = *(_DWORD *)(a1 + 36);   // nor.z  → m_Normal.z  (+0x14)

  // --- Face remap (m_FaceID) ---
  // Set up temporary geometry filter on stack (type=5=ePxTriangleMesh, default 1.0f scale)
  // Check if actor supports triangle remap: actor->vtable[15](actor)
  int hasRemap = (*(int (__fastcall **)(_DWORD))(**(_DWORD **)(a1 + 4) + 60))(*(_DWORD *)(a1 + 4));
  int faceID = -1;
  if ( hasRemap == 1 )
  {
    // Get remap table from TriangleMesh geometry: geom->vtable[13](geom) = PxTriangleMesh::getTrianglesRemap()
    int *remap = (int*)(*(int (__fastcall **)(int))(*(_DWORD *)geomOnStack + 52))(geomOnStack);
    faceID = remap[*(_DWORD *)(a1 + 8)];  // remap[faceIndex at a1+0x08]
  }
  *(_DWORD *)(a2 + 24) = faceID;  // → m_FaceID (+0x18)

  // --- Distance (m_Distance) ---
  int result = *(_DWORD *)(a1 + 40);   // distance at a1 + 0x28
  *(_DWORD *)(a2 + 28) = result;       // → m_Distance (+0x1C)
  return result;
}
```

---

## Pseudocode — Engine (x64)

```cpp
void __fastcall PxLocationHitToRaycastHit(const physx::PxLocationHit *a1, RaycastHit *a2)
{
  // --- Collider instance ID ---
  // actor = *(a1 + 0x08)   (PxRigidActor*)
  // *(actor + 16)          → Unity Object* (Collider)
  // Object::GetInstanceID(collider)
  __int64 actor = *((_QWORD *)a1 + 1);   // a1 + 0x08
  Object *collider = *(Object **)(actor + 16);
  int instanceID = collider ? Object::GetInstanceID(collider) : 0;
  *((_DWORD *)a2 + 10) = instanceID;     // a2 + 0x28 → m_Collider

  // --- Position (m_Point) ---
  // PxVec3 position at a1 + 0x1C
  *(_QWORD *)a2            = *(_QWORD *)((char *)a1 + 28);   // pos.xy → m_Point.xy (+0x00)
  *((_DWORD *)a2 + 2)      = *((_DWORD *)a1 + 9);            // pos.z  → m_Point.z  (+0x08)

  // --- Normal (m_Normal) ---
  // PxVec3 normal at a1 + 0x28
  *(_QWORD *)((char *)a2 + 12) = *((_QWORD *)a1 + 5);        // nor.xy → m_Normal.xy (+0x0C)
  *((_DWORD *)a2 + 5)      = *((_DWORD *)a1 + 12);           // nor.z  → m_Normal.z  (+0x14)

  // --- Face remap (m_FaceID) ---
  // Stack geometry: v15=5 (PxGeometryType::eTRIANGLEMESH), scale=1.0f defaults
  int faceID = -1;
  // actor->vtable[14](actor, &geomOnStack) — fills geomOnStack if shape is TriangleMesh
  if ( actor->vtable[14](actor, &geomOnStack) )
  {
    // geomOnStack->vtable[12]() → PxTriangleMesh* → getTrianglesRemap()
    int *remap = (int*)geomOnStack->vtable[12]();
    // Assert: remap != NULL
    faceID = remap[*((unsigned int *)a1 + 4)];  // remap[faceIndex at a1+0x10]
  }
  *((_DWORD *)a2 + 6) = faceID;   // a2 + 0x18 → m_FaceID

  // --- Distance (m_Distance) ---
  *((_DWORD *)a2 + 7) = *((_DWORD *)a1 + 13);  // a1 + 0x34 → m_Distance (+0x1C)
}
```

---

## physx::PxLocationHit struct layout

Derived from observed memory accesses on both platforms.

### Android (armeabi-v7a) — 32-bit

```
struct PxQueryHit {
    PxShape*      shape;        // +0x00 (4 bytes)
    PxRigidActor* actor;        // +0x04 (4 bytes) ← collider lookup
    uint32_t      faceIndex;    // +0x08 (4 bytes) ← face remap input
};

struct PxLocationHit : PxQueryHit {
    PxHitFlags    flags;        // +0x0C (2 bytes, uint16)
    uint8_t       pad[2];       // +0x0E
    PxVec3        position;     // +0x10 (12 bytes) ← m_Point
    PxVec3        normal;       // +0x1C (12 bytes) ← m_Normal
    float         distance;     // +0x28 (4 bytes)  ← m_Distance
};
// total: 0x2C bytes
```

### Windows (x64)

```
struct PxQueryHit {
    PxShape*      shape;        // +0x00 (8 bytes)
    PxRigidActor* actor;        // +0x08 (8 bytes) ← collider lookup
    uint32_t      faceIndex;    // +0x10 (4 bytes) ← face remap input
    uint32_t      pad;          // +0x14 (4 bytes alignment)
};

struct PxLocationHit : PxQueryHit {
    PxHitFlags    flags;        // +0x18 (2 bytes, uint16)
    uint8_t       pad2[2];      // +0x1A
    PxVec3        position;     // +0x1C (12 bytes) ← m_Point
    PxVec3        normal;       // +0x28 (12 bytes) ← m_Normal
    float         distance;     // +0x34 (4 bytes)  ← m_Distance
};
// total: 0x38 bytes
```

---

## Field mapping: PxLocationHit → RaycastHit

| RaycastHit field | Offset | Android source      | Windows source        |
|-----------------|--------|---------------------|----------------------|
| `m_Point`       | +0x00  | `a1 + 0x10` (pos xyz) | `a1 + 0x1C` (pos xyz) |
| `m_Normal`      | +0x0C  | `a1 + 0x1C` (nor xyz) | `a1 + 0x28` (nor xyz) |
| `m_FaceID`      | +0x18  | `remap[a1 + 0x08]` or `-1` | `remap[a1 + 0x10]` or `-1` |
| `m_Distance`    | +0x1C  | `a1 + 0x28`         | `a1 + 0x34`           |
| `m_UV`          | +0x20  | **NOT written here** — filled by caller | same |
| `m_Collider`    | +0x28  | `*(*(actor+8) + 4)` | `GetInstanceID(*(actor+16))` |

---

## Face remap logic

**Purpose:** PhysX may reorder triangles during mesh cooking (BVH optimization). `faceIndex` in the hit result is the cooked index, not the original mesh triangle index. The remap table converts cooked → original.

**When remap applies:** Only for `PxTriangleMeshGeometry` (type=5) and `PxHeightFieldGeometry`. For primitives (sphere, capsule, box, convex) → `m_FaceID = -1`.

```
actor → get geometry → if TriangleMesh:
    PxTriangleMesh::getTrianglesRemap() → uint32_t remap[]
    m_FaceID = remap[hit.faceIndex]
else:
    m_FaceID = -1
```

**Vtable slots for remap check:**

| Platform    | Vtable offset | Slot | Method |
|-------------|---------------|------|--------|
| armeabi-v7a | `+60`         | 15   | Check if TriangleMesh geometry (returns 1 = yes) |
| x64         | `+112`        | 14   | `getGeometry(PxGeometryHolder&)` variant for TriangleMesh |

**Vtable slots for remap table:**

| Platform    | Vtable offset | Slot | Method |
|-------------|---------------|------|--------|
| armeabi-v7a | `+52`         | 13   | `PxTriangleMesh::getTrianglesRemap()` |
| x64         | `+96`         | 12   | `PxTriangleMesh::getTrianglesRemap()` |

---

## Collider retrieval chain

| Platform    | Chain |
|-------------|-------|
| armeabi-v7a | `*(PxRigidActor + 0x08)` → wrapper ptr → `*(wrapper + 0x04)` = instance ID (direct field) |
| x64         | `*(PxRigidActor + 0x10)` → `Object*` (Collider) → `Object::GetInstanceID()` |

The intermediate wrapper on Android (vs direct Object* on Windows) is likely due to the 32-bit Unity object model using a different reference scheme.

---

## Function rename (Android IDB)

| Original     | Rename                    | Reason |
|--------------|---------------------------|--------|
| `sub_6424F0` | `PxLocationHitToRaycastHit` | Confirmed by Windows symbol name; cross-validated from call in `sub_643F00` |
