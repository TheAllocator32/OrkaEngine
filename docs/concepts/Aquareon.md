# Orka **Aquareon**
A voxel grid based **fluid simulation** technique.

### How it works:

Each object in the simulation owns a **FluidMap** – a 3D array (values 0–255) representing the local fluid concentration per voxel.  
Fluid behavior is driven by a set of diffusion and gravity rules evaluated in real time.

#### Fluid Diffusion
- Fluid flows to neighboring voxels once a **threshold value** (128) is exceeded.
- Valid neighbors are defined only along the **X**, **Z**, and **negative Y** directions (no upward diffusion).
- Neighboring voxels must be **in contact with solid geometry**.
- If a fluid value **exceeds 255**, overflow is allowed to propagate into empty space, creating **dripping effects**.

#### Gravity Priority
- When the voxel directly below (−Y) is free, and the fluid value is above **64**, fluid preferentially flows downward to simulate gravity.

#### Collision Handling
- To avoid expensive runtime raycasts, a **FluidCollisionLookupTable** is generated at startup.
- This table defines voxel–mesh intersections and contact states for efficient collision queries during the simulation.

#### Animated & Moving Objects
- Each moving or animated object maintains its own **FluidMap**, transformed into **global coordinates** for simulation consistency.
- For **skinned meshes**, each bone can have its own **FluidMap** region.
- These maps are stored in **hash-based structures** to minimize memory usage.

#### Contact Detection
- Voxels are considered "in contact" if they **intersect or touch** the object's mesh surface.
- This ensures fluid adheres naturally to complex geometry without redundant checks.

---

### How I will implement it
- **FluidCollisionLookupTable** generation at scene startup
- Actual computation in compute shaders
- Visualization via fluid shader

---

### Visualization
- Every voxel gets a ball as a shape
- If the ball has neighbors it will be scaled by `sqrt(voxelSize)` in the directions of all neighbors (by => `1+val`)
- If the ball has no neighbor in the positive X direction it will be scaled in that direction by `fluidValue/256*1.25`

---

### Limitations:
- Not 100% realistic due to voxels
- No real fluid solver (no pressure, real waves, ...) - made for convincing rain, paint, slime and other surface fluids
- More voxels = more GPU power!
- Complicated for moving objects
- As for now, fluids **do not interact** with other objects (e.g. rigid bodies won’t push away liquids) -> Made for e.g. realistic rain, paintball, …
- Hard to implement… Even compared to SPH.

---

### Settings
- **Viscosity** can be changed (by modifying the **fluid threshold**)
- **Color** can be changed
- **Gravity** can be changed

> You can simulate paint, slime, water, mud, rain, or pretty much any surface fluid you want!