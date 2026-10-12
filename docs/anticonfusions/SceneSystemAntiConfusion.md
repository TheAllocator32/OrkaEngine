Today we are anti-confusing:
## 🌀 Model vs. Mesh vs. Skeleton vs. ModelScenenode vs. GLTF vs. whatever
> *Because naming things is hard — and loading them even harder...*

### 📦 **Model**
The **Model** in Orka is the **top-level 3D asset** you load.  
It contains:
- One or more **Meshes** (what other engines might call *submeshes*)
- Optionally a **Skeleton** (for skinning & animation)

You instantiate the **Model** in a scene by attaching it to a `ModelScenenode`.

---
### 🔩 **Mesh**
A **Mesh** is the **geometric part** of a Model.
- It holds the vertex/index buffers
- Has its own **material / shader slot**
- Is never used directly in the scene — it always belongs to a Model

In many other tools a “Mesh” can itself contain several sub-parts.  
In Orka the word **Mesh ≈ Submesh** elsewhere.

---
### 🦴 **Skeleton**
A **hierarchical bone structure** stored inside the Model.
- Drives animation and skinning
- Shared by all the Meshes of the Model
- Lives at the Model level, not in each Mesh
- A model may only reference one Skeleton

---
### 🌳 **ModelScenenode**
A **scene graph node** that **references a Model**.
- Gives the Model a **position, rotation, scale**
- Is what you actually spawn and manipulate inside the scene
- Allows the same Model asset to be reused by many scene nodes

---
### 🏛️ **Scene**
An optional **group** of SceneNodes! Not an isolated world instance.
- Lets you organize SceneNodes 
- ... and eventually assign a dedicated PhysicsWorld and RenderTarget (e.g for character preview) (feature planned for *Coral Reef* aka Orka 4.x)
- You can load multiple Scenes at once since **they are only groups**!

---
### 📦 GLTF Conventions vs Orka
GLTF uses:
- `mesh` → roughly maps to **Orka Model**
- each `primitive` inside a GLTF mesh → maps to **Orka Mesh**
- `node` → similar to **ModelScenenode** (can carry a mesh reference or act as a bone)

GLTF’s skeleton/joint system maps naturally onto Orka’s Skeleton.

---

> **🤔 WHY!!??**  
> Well... I thought otherwise Mesh would be pretty useless... Imagine a Model only containing a single Mesh at once. Then Mesh would only act as **wrapper around its Submeshes**. You get it? That would feel pretty confusing and redundant in my opinion, so I removed the Mesh wrapper layer.