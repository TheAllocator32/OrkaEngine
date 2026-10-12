# Hello guys :)
This entire folder contains handwritten Orka docs that combine a user manual, some project philosophy (so you understand why I did things the way I did and don't complain later) and some useful tips.
Before we start, I wanted to bore you with some philosophy behind those docs:

You probably all know this kind of useless auto-generated documentation that is generated from header comments etc… To be serious… After reading those auto-generated docs I always think I could have learned more if I had just read the header instead. So instead of telling you exactly what the headers say, I made something you'll hopefully find useful (and enjoy reading too 😉). Please appreciate this, it was seriously hard work, pain and love I put into this. I wanted to do this myself (and did not delegate this job to an AI 😁) because there is so much philosophy and so many design decisions I had to wrestle with myself.

## Let's start
You probably want to start by installing Orka. It is pretty easy. If you want to go the easy way, read
- [Installation](installation/Installation.md)

But if you say "cmon I want to suffer CMake and build-systems", read
- [Manual Installation](installation/ManualInstallation.md)

We should probably continue with an
## Introduction to Orka's Scene system
If you are familiar with common scene conventions and naming (just as in gltf or other engines), you probably need some **AntiConfusion™** first because Orka is a bit different:
- [Scene System AntiConfusion](anticonfusions/SceneSystemAntiConfusion.md)
After that you may directly start reading detailed API docs
- [3D subsystem](subsystems/3D.md)
Or continue with a Tutorial and read the API docs in parallel or when you actually need them
- [Tutorial](tutorials/SomeTutorial.md)

Next up:
## Orka's subsystems
*In alphabetical order*
- [3D subsystem](subsystems/3D.md) - You should know it already
- [Audio subsystem](subsystems/Audio.md) - Sound playback and 3D sounds
- [Essentials](subsystems/Essentials.md) - Logger, ProgressManager etc.
- [Gameplay subsystem](subsystems/Gameplay.md) - CharacterController, GameMode etc.
- [GUI subsystem](subsystems/GUI.md) - GUIPage/Widget, RMLGUIPage and other GUI stuff
- [HardwareJudger™](subsystems/HardwareJudger.md) - Uhm...
- [Input subsystem](subsystems/Input.md) - InputHandler etc.
- [Leviathan](subsystems/Leviathan.md) - Leviathan Networking
- [Leviathan Server](subsystems/LeviathanServer.md) - Leviathan Relay server reference implementation
- [Networking subsystem](subsystems/Network.md) - HTTP and UDP server/client etc.
- [Physics Engine](subsystems/Physics.md) - Bullet based Physics subsystem
- [Scripting subsystem](subsystems/Scripting.md) - Advanced wrapper around AngelScript and Lua
- [Tools](subsystems/Tools.md) - Gedöns
- [WhaleMLFile class](subsystems/WhaleMLFile.md) - Convenience wrapper around the WhaleML parser
- [Window class](subsystems/Window.md) - For managing the window