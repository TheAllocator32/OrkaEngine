# Orka File Formats

Orka uses WhaleML based configuration/gameplay formats.

## Current formats
- *.player.whaleml* for player config
- *.actions.whaleml* for character actions
- *.upgrades.whaleml* for upgrades
- *.music.whaleml* for music config
- *.sound.whaleml* for 3D sound scene node descriptions
- *.scene.whaleml* for SceneNode clustering (and *.chunk.whaleml*)
- *.model.whaleml* for model scene node descriptions
- *.light.whaleml* for light scene node descriptions
- *.rigidbody.whaleml* for rigid body settings
- *.softbody.whaleml* for soft body settings
- *.ghostobject.whaleml* for ghost object settings
- *.world.whaleml* for physics world settings

---
## .player.whaleml
```whaleml
abilities.file : "soldier.actions.whaleml"
upgrades.file : "soldier.upgrades.whaleml"
music.file : "soldier.music.whaleml"
scenenode.file : "soldier.model.whaleml"
script.file : "soldier.as"
```
---
## .actions.whaleml
Use direct children under `actions`. The child name is the action id.
```whaleml
actions
    idle
        func : "idle"
    walk
        func : "walk"
    jump
        func : "jump"
    primaryAttack
        func : "shoot"
        extraargs : "PaintballSimulator@, int"
```
Compatibility note: indexed fallback is still supported, e.g. `actions.0.id`, `actions.0.func`.

---
## .upgrades.whaleml
Use direct children under `upgrades`. The child name is the upgrade id.
```whaleml
upgrades
    running_speed
        name : "Running Speed"
        description : "Increase running speed while moving"
        func : "runningspeed"
        levels : 5
        steps : 20, 50, 90, 160, 200
    reload_speed
        name : "Reload Speed"
        description : "Increase reload speed"
        func : "reloadspeed"
        levels : 20
        steps : 20, 50, 90, 160, 200, 300, 350
```
Compatibility note: indexed fallback is still supported, e.g. `upgrades.0.id`, `upgrades.0.func`.

---
## .music.whaleml
A music file defines exactly one audio entry.

```whaleml
audio.file : "lobby.ogg"
audio.loop : true
audio.gain : 0.8
loop.point : 0.0 // Seconds from the beginning; loops from here to the end when audio.loop is true
meta.name : "Lobby Theme"
meta.artists : "Lucas"
meta.date : "2026"
```
---
## .sound.whaleml
A sound file describes one 3D audio source for `AudioSceneNode::loadFile()`.
```whaleml
audio.file : "door_open.ogg"
audio.loop : false
audio.gain : 0.8
loop.point : 0.25 // Seconds from the beginning; loops from here to the end when audio.loop is true
script.file : "door_sound.lua"
script.instance : true
```
---
## .scene.whaleml & .chunk.whaleml
A scene file describes a group of SceneNodes and their transform

### Option 1 - straightforward
```whaleml
scene.name : "Test Scene"
scene.lod.distance : 300, 400, 800 // LOD distance thresholds. If more LOD stages exist than thresholds, Orka continues the last distance step. No default.
scene.objects
    oak1
        type : model // You could leave it away here because type defaults to model
        file : "data/trees/oak21.model.whaleml"
        position : x, y, z
        rotation : w, x, y, z // Also supports rotation by radians (if no 'w' is present/you pass 3 values)
        scale : x, y, z
    thesacredmainspotlight // You choose the names yourself
        type : light
        file : "data/lights/spotlight1.light.whaleml"
        position : x, y, z
        rotation : w, x, y, z
    waterfallsound
        type : sound
        file : "data/sounds/waterfall.sound.whaleml"
        position : x, y, z
// Particle emitter, billboard and trigger area coming soon :)
```
### Option 2 - chunked
```whaleml
scene.name : "Test Scene"
scene.lod.distance : 300, 400, 800
scene.chunks
    MyStupidIsland
        file : "data/worlds/MyStupidIsland.chunk.whaleml"
        position : x, y, z // Chunks can't be rotated or scaled!
        viewdistance : 3000 // Useful if you e.g, want the mountain to be visible miles away but want to hide that small house back there. Defaults to infinite!
    TheLakeAroundIt
        file : "data/worlds/TheLakeAroundIt.chunk.whaleml"
        position : x, y, z
        viewdistance : infinite
```

#### *.chunk.whaleml*
```whaleml
chunk.name : "A Lake lol"
chunk.objects
... // The rest is identical to .scene.whaleml
```

#### Important:
- Chunks may not have subchunks!!
- Object transforms inside a chunk are local to the chunk position.
- Chunks cannot be rotated or scaled. Physics already suffers enough. If you need rotating islands, rotate the planet instead :/
---
## .model.whaleml
```whaleml
 name : "Example node"
 
 model.file : "robot.glb"
 model.lod.auto : off // Defaults to 'on' if no custom LOD stages defined
 model.lods : "robot_lod1.glb", "robot_lod2.glb", "robot_lod3.glb" // lod 1 is the first real LOD stage because "robot.glb" would be LOD0 (used if very close)
 
 physics.body.type : rigid
 physics.body.settings : "robot.rigidbody.whaleml"
 
 shader.type : custom // May be pbr basic unlit toon custom
 shader.file : "shaders/shader" // Leave away the file extension! Only required if shader.type custom is selected
 
 script.file : "script.lua"
 script.instance : true // Whether to instance the script or share it with other SceneNodes using the same script path (see docs/3D.md), defaults to true
 
 userdata.string : "Custom user data string"
 userdata.file : "custom_userdata.sql"
```
---
## .light.whaleml
```whaleml
name : "Main Sun"

light.type : point // Supports point and spot
light.color : 1.0, 0.95, 0.85 // RGB color, supports float (0.0-1.0) or uint8 (0-255)
light.intensity : 2.0 // Multiplier to make the light brighter. Defaults to 1.0.

light.range : 25.0 // Use this to tune the maximum distance the light can reach. Required/No Default!

// Spot only
spot.inner_cone : 25.0 // Defines the cone in which the light is 100% visible. In degrees. No default.
spot.outer_cone : 40.0 // Defines the cone in which the light will fade from inner cone to 0% (as fade). Defaults to inner_cone

shadow.enable : true
shadow.resolution : 2048 // Defaults to 1024

script.file : "sun.lua"
script.instance : true
```
---
## .rigidbody.whaleml
```whaleml
mass : 1.0
friction : 0.5
restitution : 0.0
collision_shape : "box" // box sphere capsule convexhull convexdecomposition mesh staticmesh trimesh
box_size : 1.0, 2.0, 1.0 // Optional for box (auto from mesh if omitted)
sphere_radius : 0.75 // Optional for sphere (auto from mesh if omitted)
capsule_radius : 0.5 // Optional for capsule
capsule_height : 1.0 // Optional for capsule
linear_damping : 0.01
angular_damping : 0.02
rolling_friction : 0.0
spinning_friction : 0.0
ccd : false
ccd_motion_threshold : 0.0000001
ccd_swept_sphere_radius : 0.2
kinematic : false
```
---
## .softbody.whaleml
```whaleml
mass : 1.0
linear_stiffness : 0.5
angular_stiffness : 1.0
volume_stiffness : 1.0
damping : 0.01
friction : 0.5
pose_matching : 0.0
pressure : 0.0
bending_constraints : 2
```
---
## .ghostobject.whaleml
```whaleml
collision_shape : "box" // box sphere capsule convexhull convexdecomposition mesh staticmesh trimesh
box_size : 1.0, 2.0, 1.0 // Optional for box (auto from mesh if omitted)
sphere_radius : 0.75 // Optional for sphere (auto from mesh if omitted)
capsule_radius : 0.5 // Optional for capsule
capsule_height : 1.0 // Optional for capsule
```
---
## .world.whaleml
```whaleml
gravity : -9.81
solverIterations : 14
splitImpulse : true
air_density : 1.2
water_density : 0.0
water_offset : 0.0
water_normal : 0.0, 0.0, 0.0

threading.multithreaded : false // default fallback for dispatcher+solver if specific keys are omitted
threading.multithreaded_dispatcher : false // use btCollisionDispatcherMt when true
threading.multithreaded_solver : false // use btSequentialImpulseConstraintSolverMt when true
```

Compatibility aliases (still accepted):
- `multithreaded` -> same as `threading.multithreaded`
- `multithreaded_dispatcher` -> same as `threading.multithreaded_dispatcher`
- `multithreaded_solver` -> same as `threading.multithreaded_solver`

> File formats shall only be updated by project leaders/maintainers!