# Orka Engine – the Game Engine that stays out of your way
**A lightweight, flexible, manageable, extendable, dev-friendly backend game engine for experienced C++ devs**

### 🌊 Version 2.0 “Spring Tide” (Beta stage)

> This README targets **Orka 2.1** after release so don't wonder if some features from the ***Features*** section don't exist yet or are work-in-progress

---
### What Orka is (and... isn’t) 
- Orka is a **C++ backend engine/library** — **not** a full editor like Unity or Godot
- You build your game **on top of Orka**, not inside it
- Designed for **experienced C++ developers** who want full control over architecture

> Think: foundation, not finished house.

---
## Features
### API
- Easy to use **developer friendly**
- **C++23**
- Mostly **thread-safe**
- **High Level** but
- **Skippable abstraction levels** (e.g. Skip Mesh and directly access Renderer)
- no `getThis()->getThat()->ohDamnIHatePointersAAAAH()` hell I promise ;)
### Core
- A custom lightweight **PBR capable renderer** based on BGFX
- A simple, flat **Scene interface** (*SceneManager*, *SceneNode*, *Camera*)
- A **Rigid/Soft Body Physics system** based on Bullet Physics
- Built-in **Audio support** (OGG, MP3, FLAC, WAV) supporting **3D effects** (doppler, reverb, etc.)
- **Native GUI rendering**: Both *Dear ImGui* and *RmlUI* are directly integrated and can be used out of the box
- Support for **many different platforms** (Windows, Linux, MacOS, currently supported)
- An advanced **scripting system** (using ***Lua*** and/or ***AngelScript*** depending on what you choose) designed to build **modular games**
- Human friendly [**WhaleML**](docs/concepts/WhaleML.md) DSL
### Additional stuff
- A simple **translation/localization** system supporting .mo/.po files
- Utility classes for common gameplay functionality, **simplifying** complex tasks (e.g. *CharacterController*, *WorldMap* and *GameMode*)
- A **Cryptography** system that can **encrypt and decrypt** strings and even perform server-client handshakes.
- A fairly **low level networking system** supporting **ENet (UDP)** and **HTTP/HTTPS** Server/Client
- Many other small but useful systems (like ProgressManager, FileSystem abstraction, InputManager, etc.)
### Tools
- A **shader compilation tool** generating shader headers during build and runtime shader binaries post-install (See [OrkaShaderC](shaderc/AntiConfusionAndReadme.md))
- A **Blender addon** that is able to export **Blender scenes** to Orka compatible Scene formats
- A **wizard** for project quickstart
### Selling points
- The famous **Hardware Judger**: One of the (too) few benchmarking tools that roast your PC in plain english
- A custom **Networking system** called [**Leviathan**](docs/concepts/Leviathan.md)
- Insanely **small binary** size of around **20-30MB** with all features compiled
- Handwritten **[documentation](docs/OrkaDocumentation.md)** (no doxygen jungle)
- Features can be disabled to shrink down the binary even more

---
## Scripting Philosophy
- Orka is designed as a **C++-only game engine**. That’s why:

> **Scripts** are **data**
- That doesn't mean "scripts are useless" or "scripts are no executable code" but:
- Scripts only **describe actions and reactions** of **specific assets** (e.g. movement of player xy or onCollision with object xy in world foo)
- ... and may **NOT contain gameplay core code** 

- Only certain systems are scriptable; that’s a **feature**, not a bug!

**The game defines game logic; Orka provides (some?) systems; Scripts define asset behaviour!**

Confused now? Read examples in [Scripting dos and don'ts](docs/concepts/ScriptingExamples.md)

---
## Example code
```cpp
#include <Orka.hpp>

int main(int argc, char* argv[]) {
    OrkaEngine::init("Orka Test"); // Initialize Orka
    OrkaEngine::intro(); // Show splash

    ModelSceneNode& sceneNode = ModelSceneNode::create(); // SceneManager owns the object. Use destroy() only when you want to remove it early.
    sceneNode.loadFile("path/whatever.model.whaleml"); // Loads a .model.whaleml
    PhysicsEngine::applyCentralImpulse(sceneNode, vector3df(100,0,0)); // Apply some impulse

    OrbitCameraSceneNode& camera = OrbitCameraSceneNode::create();
    camera.setOrbitSpeed(1);
    camera.setOrbitObject(sceneNode);

    while(Window::isOpen()) {
      float delta = Timing::getDelta("Main");
      OrkaEngine::update(delta);
    }
    
    OrkaEngine::deinit();
}
```
- This minimal example opens a window named "Orka Test"
- Shows Orka splash and plays intro emblem
- Loads `path/whatever.model.whaleml` and shows it on screen (See [model.whaleml docs](docs/FileFormats.md))
- Applies impulse to it
- Creates camera orbiting around it
- enters frame loop until exit

---
## Quickstart
- Download and run `wizard.py` and follow its instructions
- For manual installation see [manual installation](docs/installation/ManualInstallation.md)

---
## License
Orka Engine is licensed under the **Apache License Version 2.0**.
### What does this mean?
- **You CAN use Orka Engine in commercial projects.** (Which might surprise you)
- **Modifications to Orka Engine don't have to be Open Source.** (But I would be really happy if you share them 😁)
- **Your game itself does NOT have to be Open Source.** (Which is good for our closed source devs)
- **No one can register patents on Orka Engine.** (Which is good for my sanity)

📜 See the file **[LICENSE](LICENSE)** in the project root for full details.

___
## Who did this?
Orka is a hobby project created and maintained by **Lucas G.** (aka *TheAllocator32*).
It was originally built for a game I wanted to create... and
#### *I wanted full control - without fighting my engine*

Moreover:
- Those big engines are great, but I kept ending up in situations where I was fighting them instead of making a game (I tried to use Godot as C++ backend library, ouch...)
- Their runtime is full of stuff you will probably never use
- And... This... "Everything is a script"-concept doesn't fit my needs

So I started building my own Game Engine that does exactly what I need (and much more!), in exactly the way I (and maybe you too 😁) want it to.

> If you haven't already noticed: Orka **was never meant to be a replacement** for any other Game engine! I created it because other engines didn’t fit **my** workflow!

___
## Feature Roadmap

*Since Orka is still in development, this roadmap lists all (major) features and their current status*

| Feature                | Status | Notes                                    |
|------------------------|--------|------------------------------------------|
| Renderer               | ✅️🛠️   | Shadows and PBR still missing            |
| Scene System           | ✅️🛠️   | Some SceneNode types still missing       |
| Physics                | ✅️🛠️🐉   | Only Rigidbody works so far              |
| Hardware Judger        | ✅️🐉🐉 | Humiliating your PC 💻🔥 (Wanted Dragon) |
| Networking             | ✅️🐉   | Working, still not bullet proof          |
| ImGUI GUI System       | ✅️     | Works great                              |
| RmlUI GUI System       | ✅️     | Now works as intended                    |
| Scripting              | ✅️     | Works 👌                                 |
| Audio                  | ✅️✅️   | Playing audio since 2023 😎              |
| Crypto                 | ✅️     | Encrypts like a bitcoin ASIC 🔒          |
| Game Mode              | ✅️     | Brutally simplified, almost idiot proof! |
| Leviathan              | 🛠️     | In development (see #42)                 |
| Fileformats/Parsers    | ✅️     | Stable and blazingly fast                |
| Leviathan server       | ✅️🛠️   | Grows as Leviathan grows                 |
| Cross Platform Support | (✅️)🛠️💥️ | Tested Linux and Windows.                |

#### Overall Orka status: Almost everything in order! On our way to release candidate 2.1 🎉!

**Legend:**
- 🛠️ — under development  
- ♻️ — being re-implemented  
- ✅️ — finished/stable
- ❌️ — not implemented / abandoned
- 💥️ - Dynamite (may blow up your compiler)
- 🐉 - Dragon (may defeat your sanity)

___
## Versioning
- Versions start with a **series** (0.x 1.x) and continue with a **version** (anything >1) (version 0 means WIP before first release)
- Inside a series (above 0.x), the API may *enhance and change* but must be at least **backward compatible** with the *first version* of the series
- Orka starts up from 0.x, called **Echo Layer 🌀**, which is the first series full of development chaos
- Once finished Orka will turn 1.x called **Thermocline 🔥** <= Current stable release
- It continues with 2.x called **Spring Tide 🌊**  <= Current Status (in development, unstable)
- 3.x called **Pycnocline 🫧**
- And 4.x named **Coral Reef 🪸**
- Because I like the name: 5.x will be **Hadal Reach 🦑** 
- There will be further versions, but that’s enough names for now, I guess…

> Note: Orka does not use Semantic Versioning! Versions ending in *.0* mark the development phase of a new series (I call them the ***chaos phase***). The first public release of a series is always *.1*.

---
### Congrats, you made it to the end of the README! 🎉


> This readme was not made by ChatGPT, the emojis were my idea!
