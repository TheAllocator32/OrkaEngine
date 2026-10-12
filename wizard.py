#!/usr/bin/env python3
import os
import shutil
import subprocess
import sys
from pathlib import Path

ORKA_REPO_URL = "https://github.com/TheAllocator32/OrkaEngine.git"
USE_COLOR = sys.stdout.isatty() and os.environ.get("NO_COLOR") is None


def style(text: str, code: str) -> str:
    return f"\033[{code}m{text}\033[0m" if USE_COLOR else text


def muted(text: str) -> str:
    return style(text, "2")


def accent(text: str) -> str:
    return style(text, "1;36")


def success(text: str) -> str:
    return style(text, "1;32")


def warning(text: str) -> str:
    return style(text, "1;33")


def failure(text: str) -> str:
    return style(text, "1;31")

SCRIPTING_CHOICES = {
    "1": ("both", True, True),
    "2": ("lua", True, False),
    "3": ("angelscript", False, True),
    "4": ("none", False, False),
}

SUBSYSTEMS = [
    {
        "key": "audio",
        "label": "Audio",
        "cmake": "ORKA_ENABLE_AUDIO",
        "default": True,
        "depends": [],
        "description": "Audio playback and music helpers.",
    },
    {
        "key": "physics",
        "label": "Physics",
        "cmake": "ORKA_ENABLE_PHYSICS",
        "default": True,
        "depends": [],
        "description": "Bullet physics and SceneNode physics integration.",
    },
    {
        "key": "crypto",
        "label": "Crypto",
        "cmake": "ORKA_ENABLE_CRYPTO",
        "default": True,
        "depends": [],
        "description": "Sessions, hashes, and encryption helpers.",
    },
    {
        "key": "networking",
        "label": "Networking",
        "cmake": "ORKA_ENABLE_NETWORKING",
        "default": True,
        "depends": ["crypto"],
        "description": "HTTP and realtime networking.",
    },
    {
        "key": "leviathan",
        "label": "Leviathan",
        "cmake": "ORKA_ENABLE_LEVIATHAN",
        "default": True,
        "depends": ["networking"],
        "description": "Leviathan client and relay server networking.",
    },
    {
        "key": "gui",
        "label": "GUI",
        "cmake": "ORKA_ENABLE_GUI",
        "default": True,
        "depends": [],
        "description": "ImGui/RmlUi rendering and widgets.",
    },
    {
        "key": "gameplay",
        "label": "Gameplay",
        "cmake": "ORKA_ENABLE_GAMEPLAY",
        "default": True,
        "depends": ["physics", "scripting"],
        "description": "GameMode, Player, upgrades, and character controller.",
    },
]

RENDERER_FEATURES = [
    ("pbr", "PBR materials", True),
    ("skinning", "Skinning / skeletons", True),
    ("meshopt", "Meshoptimizer decoding", True),
]


def ask(prompt: str, default: str | None = None) -> str:
    if default is not None:
        value = input(f"  {prompt} {muted(f'[{default}]')}  ").strip()
        return value or default
    return input(f"  {prompt}  ").strip()


def ask_yes_no(prompt: str, default: bool = True) -> bool:
    suffix = "Y/n" if default else "y/N"
    while True:
        value = input(f"  {prompt} {muted(f'[{suffix}]')}  ").strip().lower()
        if not value:
            return default
        if value in {"y", "yes", "true", "1", "on"}:
            return True
        if value in {"n", "no", "false", "0", "off"}:
            return False
        print(f"  {warning('!')} Please answer yes or no.")


def print_banner():
    print()
    print(f"  {accent('ORKA')} {muted('/')} {style('WAVE BREAKER', '1')}")
    print(f"  {muted('Configure, generate, and bootstrap an Orka project.')}")


def print_section(title: str):
    print(f"\n  {accent('◆')} {style(title.upper(), '1')}")


def print_info(message: str):
    print(f"  {accent('›')} {message}")


def print_success(message: str):
    print(f"  {success('✓')} {message}")


def print_warning(message: str):
    print(f"  {warning('!')} {message}")


def print_error(message: str):
    print(f"  {failure('×')} {message}")


def print_status(label: str, enabled: bool, detail: str = "", indent: int = 2):
    state = success("ON ") if enabled else muted("OFF")
    suffix = f"  {muted(detail)}" if detail else ""
    print(f"{'  ' * indent}{state}  {label}{suffix}")


def ask_choice(prompt: str, options: list[tuple[str, str, str]], default: str) -> str:
    for key, label, description in options:
        marker = accent("●") if key == default else muted("○")
        print(f"    {marker} {style(key, '1')}. {label:<16} {muted(description)}")
    valid = {key for key, _, _ in options}
    while True:
        value = ask(prompt, default)
        if value in valid:
            return value
        print_warning(f"Choose one of: {', '.join(sorted(valid))}")


def run(cmd, cwd=None):
    print(f"\n  {accent('$')} {muted(' '.join(map(str, cmd)))}")
    subprocess.run(cmd, cwd=cwd, check=True)


def write_file(path: Path, content: str):
    path.write_text(content, encoding="utf-8")
    print_success(f"Created {path}")


def validate_cpp_standard(value: str) -> str:
    try:
        num = int(value)
    except ValueError:
        raise ValueError("C++ standard must be a number like 23 or 26")

    if num < 23:
        raise ValueError("Orka requires C++23 or newer.")
    return str(num)


def resolve_dependencies(enabled: dict[str, bool]) -> list[str]:
    notes: list[str] = []
    changed = True
    while changed:
        changed = False
        for subsystem in SUBSYSTEMS:
            key = subsystem["key"]
            if not enabled.get(key, False):
                continue
            for dep in subsystem["depends"]:
                if dep == "scripting":
                    if not enabled.get("scripting", False):
                        enabled["scripting"] = True
                        notes.append("Enabled Scripting because Gameplay depends on it.")
                        changed = True
                    continue
                if not enabled.get(dep, False):
                    enabled[dep] = True
                    dep_label = next((s["label"] for s in SUBSYSTEMS if s["key"] == dep), dep)
                    notes.append(f"Enabled {dep_label} because {subsystem['label']} depends on it.")
                    changed = True
    return notes


def create_main_cpp(project_name: str, config: dict) -> str:
    if config["headless"]:
        return f'''#include <Orka.hpp>

int main(int argc, char* argv[]) {{
    (void)argc;
    (void)argv;

    OrkaEngine::init("{project_name}");

    // Start and update your networking/server logic here.

    OrkaEngine::deinit();
    return 0;
}}
'''

    if config["umbrella_header"]:
        includes = "#include <Orka.hpp>"
    else:
        includes = "\n".join([
            "#include <orka/essentials/Timing.hpp>",
            "#include <orka/window/Window.hpp>",
            "#include <orka/renderer/Renderer.hpp>",
            "#include <string>",
        ])

    if config["auto_init"]:
        init_block = f'''    OrkaEngine::init("{project_name}");
    OrkaEngine::intro();'''
        loop_update = "        OrkaEngine::update(delta);"
        deinit_block = "    OrkaEngine::deinit();"
    else:
        init_block = f'''    g_appName = "{project_name}";
    Window::create(g_appName);
    Renderer::init(RST_AUTO);'''
        loop_update = '''        Window::update();
        if (!Window::isOpen()) {
            break;
        }
        Renderer::frame();'''
        deinit_block = '''    Renderer::deinit();
    if (Window::isOpen()) {
        Window::destroy();
    }'''

    scene_sample = ""
    if config["umbrella_header"] and config["auto_init"]:
        scene_sample = '''

    HardwareJudger::printToTerminal();
'''

    return f'''{includes}

int main(int argc, char* argv[]) {{
    (void)argc;
    (void)argv;
{init_block}
{scene_sample}
    while (Window::isOpen()) {{
        float delta = Timing::getDelta("Main");
{loop_update}
    }}

{deinit_block}
    return 0;
}}
'''


def create_cmakelists(project_name: str, executable_name: str, cpp_standard: str, config: dict) -> str:
    headless_default = "ON" if config["headless"] else "OFF"
    option_lines = [
        f'option(ORKA_HEADLESS "Build the networking/essentials-only headless runtime" {headless_default})'
    ]

    if not config["headless"]:
        for subsystem in SUBSYSTEMS:
            enabled = "ON" if config["subsystems"].get(subsystem["key"], False) else "OFF"
            option_lines.append(
                f'option({subsystem["cmake"]} '
                f'"Enable Orka {subsystem["label"]} subsystem" {enabled})'
            )
        option_lines += [
            f'option(ORKA_ENABLE_SCRIPTING "Enable Orka scripting subsystem" {"ON" if config["scripting"] else "OFF"})',
            f'option(ORKA_ENABLE_LUA "Enable Lua scripting backend" {"ON" if config["lua"] else "OFF"})',
            f'option(ORKA_ENABLE_ANGELSCRIPT "Enable AngelScript scripting backend" {"ON" if config["angelscript"] else "OFF"})',
        ]

        renderer_option_names = {
            "pbr": "ORKA_ENABLE_RENDERER_PBR",
            "skinning": "ORKA_ENABLE_RENDERER_SKINNING",
            "meshopt": "ORKA_ENABLE_MESHOPT",
        }
        for key, label, _ in RENDERER_FEATURES:
            option_lines.append(
                f'option({renderer_option_names[key]} "Enable Orka {label}" '
                f'{"ON" if config["renderer_features"][key] else "OFF"})'
            )

    project_compileflags = []
    if not config["auto_init"]:
        project_compileflags.append("ORKA_MANUAL_SYSTEM_INIT")
    if not config["umbrella_header"]:
        project_compileflags.append("ORKA_NO_UMBRELLA_HEADER")

    return f'''cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_STANDARD {cpp_standard})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ENABLE_DEBUG_INFO ON CACHE BOOL "Enable debug infos in Release builds if wanted")
set(CMAKE_TOOLCHAIN_FILE "${{CMAKE_SOURCE_DIR}}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "" FORCE)
include("${{CMAKE_SOURCE_DIR}}/src/orka/orka.compilehacks.cmake")

project({project_name})

{chr(10).join(option_lines)}

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS
    "src/*.cpp" "src/*.c"
)

if(ORKA_HEADLESS)
    list(FILTER SRC EXCLUDE REGEX "/src/(gameplay|guipages)/")
    list(FILTER SRC EXCLUDE REGEX "/src/orka/(3d|audio|gameplay|gui|input|physics|renderer|scripting|window)/")
    list(FILTER SRC EXCLUDE REGEX "/src/orka/primitives/(3d|audio|gui|physics|renderer)/")
    list(FILTER SRC EXCLUDE REGEX "/src/orka/utilities/(Dialogs|ImGuiTools)\\.cpp$")
endif()

add_executable({executable_name} ${{SRC}})

include("${{CMAKE_SOURCE_DIR}}/src/orka/orka.cmake")

target_include_directories({executable_name} PRIVATE ${{ORKA_INCLUDES}})
target_link_libraries({executable_name} PRIVATE ${{ORKA_LIBS}})
target_compile_definitions({executable_name} PUBLIC ${{ORKA_COMPILEFLAGS}} {(" ".join(project_compileflags))})

if(TARGET orka_shader2code_renderer_shaders)
    add_dependencies({executable_name} orka_shader2code_renderer_shaders)
endif()
'''


def create_gitignore() -> str:
    return r'''# Build directories
build/
build-*/
cmake-build-*/
out/
dist/

# CMake
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
compile_commands.json
CTestTestfile.cmake
Testing/
Makefile
install_manifest.txt

# C++ artifacts
*.o
*.obj
*.lo
*.la
*.a
*.lib
*.so
*.so.*
*.dll
*.dylib
*.exe
*.out
*.app
*.i*86
*.x86_64
*.hex
*.elf
*.pdb
*.idb
*.ilk
*.exp

# Precompiled headers / modules
*.gch
*.pch
*.pcm

# Debug / profiling
*.dSYM/
*.su
*.gcno
*.gcda
*.gcov
*.prof
*.profraw
*.profdata

# Editors / IDEs
.vscode/
.idea/
*.vcxproj*
*.sln
*.user
*.userprefs

# Python
__pycache__/
*.pyc

# OS files
.DS_Store
Thumbs.db
'''


def create_build_sh() -> str:
    return '''#!/usr/bin/env bash
set -e

BUILD_TYPE="${1:-Release}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TOOLCHAIN_FILE="$SCRIPT_DIR/vcpkg/scripts/buildsystems/vcpkg.cmake"

echo "Configuring with CMake..."
cmake -S "$SCRIPT_DIR" -B "$SCRIPT_DIR/build" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE"

echo "Building..."
cmake --build "$SCRIPT_DIR/build" --config "$BUILD_TYPE"

echo "Done."
'''


def create_build_bat() -> str:
    return r'''@echo off
setlocal

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release
set SCRIPT_DIR=%~dp0
if "%SCRIPT_DIR:~-1%"=="\" set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%
set TOOLCHAIN_FILE=%SCRIPT_DIR%\vcpkg\scripts\buildsystems\vcpkg.cmake

echo Configuring with CMake...
cmake -S "%SCRIPT_DIR%" -B "%SCRIPT_DIR%\build" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN_FILE%"
if errorlevel 1 exit /b 1

echo Building...
cmake --build "%SCRIPT_DIR%\build" --config %BUILD_TYPE%
if errorlevel 1 exit /b 1

echo Done.
'''


def create_update_dependencies_sh() -> str:
    return '''#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

echo "Updating vcpkg repository..."
git -C vcpkg pull

echo "Checking for package updates..."
./vcpkg/vcpkg update

echo "Listing upgradable packages..."
./vcpkg/vcpkg upgrade --no-dry-run
'''


def create_update_dependencies_bat() -> str:
    return r'''@echo off
setlocal

cd /d %~dp0

echo Updating vcpkg repository...
git -C vcpkg pull
if errorlevel 1 exit /b 1

echo Checking for package updates...
call vcpkg\vcpkg.exe update
if errorlevel 1 exit /b 1

echo Listing upgradable packages...
call vcpkg\vcpkg.exe upgrade --no-dry-run
if errorlevel 1 exit /b 1
'''


def maybe_delete_dir(path: Path):
    if path.exists() and any(path.iterdir()):
        if ask_yes_no(f"'{path}' already exists and is not empty. Delete it and continue?", False):
            shutil.rmtree(path)
            print_warning(f"Removed existing directory {path}")
            return True
        return False
    return True


def run_bootstrap_if_requested(project_dir: Path):
    bootstrap = project_dir / "src" / "orka" / "bootstrap.py"
    if not bootstrap.exists():
        print_warning("bootstrap.py was not found in src/orka; skipping bootstrap.")
        return

    if not ask_yes_no("Run Orka bootstrap.py now?", True):
        return

    python_cmd = sys.executable or "python"

    try:
        run([python_cmd, str(bootstrap)], cwd=project_dir)
    except subprocess.CalledProcessError as e:
        print_error(f"bootstrap.py failed: {e}")
        print_info("You can run it manually later.")


def main():
    print_banner()

    print_section("Project")
    project_name = ask("Project name", "MyOrkaGame")
    executable_name = ask("Executable name", project_name)

    while True:
        try:
            cpp_standard = validate_cpp_standard(ask("C++ standard (23+)", "23"))
            break
        except ValueError as e:
            print_error(str(e))

    target_dir = Path(ask("Target directory", project_name)).resolve()

    print(f"    {muted('Headless projects keep essentials and networking, without graphics or gameplay.')}")
    headless = ask_yes_no("Build a headless project?", False)

    if headless:
        headless_subsystems = {"crypto", "networking", "leviathan"}
        enabled = {
            subsystem["key"]: subsystem["key"] in headless_subsystems
            for subsystem in SUBSYSTEMS
        }
        enabled["scripting"] = False
        scripting_name, lua_enabled, angelscript_enabled = "none", False, False
        renderer_features = {key: False for key, _, _ in RENDERER_FEATURES}
        dependency_notes = []
        umbrella_header, auto_init = True, True
    else:
        print_section("Scripting")
        scripting_choice = ask_choice("Scripting language", [
            ("1", "Lua + Angel", "Build both scripting backends."),
            ("2", "Lua", "Lua scripts only."),
            ("3", "AngelScript", "AngelScript scripts only."),
            ("4", "None", "Disable the scripting subsystem."),
        ], "1")
        scripting_name, lua_enabled, angelscript_enabled = SCRIPTING_CHOICES[scripting_choice]

        print_section("Subsystems")
        enabled = {"scripting": scripting_name != "none"}
        for subsystem in SUBSYSTEMS:
            depends = subsystem["depends"]
            dep_text = f" Requires {', '.join(depends)}." if depends else ""
            print(f"    {muted(subsystem['description'] + dep_text)}")
            enabled[subsystem["key"]] = ask_yes_no(
                f"Enable {subsystem['label']}?",
                subsystem["default"],
            )
        dependency_notes = resolve_dependencies(enabled)
        lua_enabled = lua_enabled and enabled["scripting"]
        angelscript_enabled = angelscript_enabled and enabled["scripting"]
        if enabled["scripting"] and not lua_enabled and not angelscript_enabled:
            lua_enabled = True
            scripting_name = "lua"
            dependency_notes.append("Enabled Lua because Scripting was required by another subsystem.")

        print_section("Renderer Features")
        renderer_features = {}
        for key, label, default in RENDERER_FEATURES:
            renderer_features[key] = ask_yes_no(f"Enable {label}?", default)

        print_section("Generated Code")
        umbrella_header = ask_yes_no("Use the Orka.hpp umbrella header?", True)
        auto_init = ask_yes_no("Use automatic engine initialization?", True)

    config = {
        "headless": headless,
        "subsystems": enabled,
        "scripting": enabled["scripting"],
        "lua": lua_enabled,
        "angelscript": angelscript_enabled,
        "renderer_features": renderer_features,
        "umbrella_header": umbrella_header,
        "auto_init": auto_init,
    }

    print_section("Review")
    for note in dependency_notes:
        print_info(note)

    print(f"\n    {style('PROJECT', '1')}")
    print(f"      Name       {project_name}")
    print(f"      Executable {executable_name}")
    print(f"      Location   {target_dir}")
    print(f"      C++        {cpp_standard}")
    print(f"      Profile    {'Headless' if headless else 'Desktop'}")

    if headless:
        print(f"\n    {style('ENGINE PROFILE', '1')}")
        print_status("ORKA_HEADLESS", True, "managed by Orka", indent=3)
        print(
            f"      {muted('Essentials, WhaleML, Crypto, Networking, and Leviathan')}"
        )
    else:
        print(f"\n    {style('ENGINE CORE', '1')}")
        print_status("Essentials", True, indent=3)
        print_status("Renderer", True, indent=3)
        print_status("WhaleML", True, indent=3)

        print(f"\n    {style('RUNTIME LAYERS', '1')}")
        print_status("Scene", True, indent=3)
        print_status("Window", True, indent=3)
        print_status("Input", True, indent=3)

        print(f"\n    {style('SUBSYSTEMS', '1')}")
        for subsystem in SUBSYSTEMS:
            depends = subsystem["depends"]
            dep_text = f"requires {', '.join(depends)}" if depends else ""
            print_status(
                subsystem["label"],
                enabled.get(subsystem["key"], False),
                dep_text,
                indent=3,
            )
        scripting_detail = scripting_name if enabled["scripting"] else ""
        print_status("Scripting", enabled["scripting"], scripting_detail, indent=3)

        print(f"\n    {style('RENDERER FEATURES', '1')}")
        for key, label, _ in RENDERER_FEATURES:
            print_status(label, renderer_features[key], indent=3)

    print(f"\n    {style('CODE GENERATION', '1')}")
    header_detail = "<Orka.hpp>" if umbrella_header else "explicit includes"
    print_status("Umbrella header", umbrella_header, header_detail, indent=3)
    print_status("Automatic initialization", auto_init, indent=3)

    print()
    if not ask_yes_no("Create project with these options?", True):
        print_warning("No files were created.")
        return

    if target_dir.exists() and any(target_dir.iterdir()):
        if not ask_yes_no(f"Directory '{target_dir}' is not empty. Continue anyway?", False):
            print_warning("No files were created.")
            return
    else:
        target_dir.mkdir(parents=True, exist_ok=True)

    print_section("Orka Source")
    mode = ask_choice("Choice", [
        ("1", "Default branch", "Clone the repository default."),
        ("2", "Branch", "Clone a named branch."),
        ("3", "Tag/version", "Clone a release tag."),
    ], "1")

    branch = None
    tag = None

    if mode == "2":
        branch = ask("Branch name", "main")
    elif mode == "3":
        tag = ask("Tag/version")
        if not tag:
            print_error("Tag/version cannot be empty.")
            return

    src_dir = target_dir / "src"
    orka_dir = src_dir / "orka"

    print_section("Create")
    print_info("Creating project files")
    src_dir.mkdir(parents=True, exist_ok=True)

    if orka_dir.exists():
        if not maybe_delete_dir(orka_dir):
            print_warning("Stopped because src/orka already exists.")
            return

    write_file(src_dir / "main.cpp", create_main_cpp(project_name, config))
    write_file(target_dir / "CMakeLists.txt", create_cmakelists(project_name, executable_name, cpp_standard, config))
    write_file(target_dir / ".gitignore", create_gitignore())
    write_file(target_dir / "build.sh", create_build_sh())
    write_file(target_dir / "build.bat", create_build_bat())
    write_file(target_dir / "update_dependencies.sh", create_update_dependencies_sh())
    write_file(target_dir / "update_dependencies.bat", create_update_dependencies_bat())

    try:
        os.chmod(target_dir / "build.sh", 0o755)
    except Exception:
        pass
    try:
        os.chmod(target_dir / "update_dependencies.sh", 0o755)
    except Exception:
        pass

    clone_cmd = ["git", "clone"]
    if branch:
        clone_cmd += ["--branch", branch]
    elif tag:
        clone_cmd += ["--branch", tag]

    clone_cmd += [ORKA_REPO_URL, str(orka_dir)]

    try:
        run(clone_cmd)
    except subprocess.CalledProcessError as e:
        print_error(f"Git clone failed: {e}")
        print_warning("Project files were created, but Orka was not downloaded.")
        return

    run_bootstrap_if_requested(target_dir)

    print_section("Ready")
    print_success(f"Project created at {target_dir}")
    print(f"""
    {muted(target_dir.name + '/')}
    ├── CMakeLists.txt
    ├── build.sh / build.bat
    ├── update_dependencies.sh / .bat
    └── src
        ├── main.cpp
        └── orka

    {style('BUILD', '1')}
      Linux/macOS  ./build.sh
      Windows      build.bat

    {style('UPDATE DEPENDENCIES', '1')}
      Linux/macOS  ./update_dependencies.sh
      Windows      update_dependencies.bat
""")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print(f"\n  {warning('!')} Cancelled.")
        sys.exit(1)
