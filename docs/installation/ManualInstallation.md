## Manual installation
- Orka works best with CMake (since it will automatically handle vcpkg)
- Orka Engine has multiple dependencies (BGFX, Bullet, Lua, etc.). To manage these easily, there's a **bootstrap.py** script included — it downloads **./vcpkg** and **installs all required libraries automatically**. You can even **modify** bootstrap.py to install extra libraries that you need in your game! Just copy this script into your project root (not inside the src folder). Run it from there before continuing
- Copy the **Orka folders** to **src/orka** (or clone this repository there...)
  - I strongly recommend keeping the Orka Engine folders in the src/orka directory; otherwise, you would have to rewrite the includes, and it's very confusing to differ between your game and Orka! (Well... maybe calling it "Orka Framework" wouldn't be too bad either! 😏).
- In the end you should have something like this:
```
project-root/
|── src/
│   |── main.cpp
│   └── orka/
│
|── bootstrap.py
|── CMakeLists.txt
```
### Add the C++ source files to your project (CMake)
I recommend **importing all C++ files** inside the `src` directory into CMake because otherwise, you would have to modify the **CMakeLists.txt** whenever you add a new source file.

```cmake
file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "src/*.cpp" "src/*.c")
```
### Now add these lines to import the libraries built by ./vcpkg:
```cmake
set(ENABLE_DEBUG_INFO ON CACHE BOOL) # Enable debug info in Release builds if you want

set(CMAKE_TOOLCHAIN_FILE "vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
include("src/orka/orka.compilehacks.cmake") # Automatically ensures C++23 as standard

project(YOURPROJECT)

# Place Orka feature flags here (if you want to disable features)

include("src/orka/orka.cmake")

include_directories(${ORKA_INCLUDES})
target_link_libraries(YOURPROJECT PRIVATE ${ORKA_LIBS})
target_compile_definitions(YOURPROJECT PUBLIC ${ORKA_COMPILEFLAGS}) # Replace YOURPROJECT with the actual name of your CMAKE project
if(TARGET orka_shader2code_renderer_shaders)
  add_dependencies(YOURPROJECT orka_shader2code_renderer_shaders) # Compile Orka Shaders
endif()
```
### When using Visual Studio please remember to disable its internal vcpkg installation; it will mess up your entire build and your sanity!

If you wish to disable some features you have to define/set orkas *feature flags* described in [Config.md](../Config.md) **before including *orka.cmake***
