#!/usr/bin/env python3
"""
Orka Engine bootstrap via vcpkg (desktop + mobile-ready)

Goals:
- Pick TARGET triplet (defaults to local desktop)
- Pick HOST triplet for build-tools (defaults to local desktop)
- Optional: --clean-after-build toggle
- Installs:
  - HOST tools (shaderc etc.) via bgfx[tools] on the HOST triplet
  - TARGET runtime deps (bgfx without tools + engine libs) on the TARGET triplet

Notes:
- iOS/Android builds should NOT try to build shaderc “on device”.
  Build shaderc on your host machine and compile shaders offline.
- For iOS you must run this on macOS with Xcode installed.
- For Android you need Android NDK installed (and usually set ANDROID_NDK_HOME).
"""

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from textwrap import dedent


# ----------------------------- helpers -----------------------------

def run(cmd, cwd=None, env=None):
    print(f"[RUN] {' '.join(cmd)}" + (f"  (cwd={cwd})" if cwd else ""))
    subprocess.run(cmd, cwd=cwd, env=env, check=True)

def which_or_die(name, hint=None):
    if shutil.which(name):
        return
    msg = f"Required tool '{name}' not found in PATH."
    if hint:
        msg += f"\nHint: {hint}"
    print(msg, file=sys.stderr)
    sys.exit(1)

def detect_os():
    s = platform.system().lower()
    if s.startswith("win"):
        return "windows"
    if s.startswith("linux"):
        return "linux"
    if s.startswith("darwin"):
        return "macos"
    return s

def detect_arch():
    m = platform.machine().lower()
    # common normalizations
    if m in ("x86_64", "amd64"):
        return "x64"
    if m in ("aarch64", "arm64"):
        return "arm64"
    if m in ("i386", "i686", "x86"):
        return "x86"
    # fall back
    return m

def vcpkg_exe_path(vcpkg_dir: Path, host_os: str) -> str:
    if host_os == "windows":
        exe = vcpkg_dir / "vcpkg.exe"
        if exe.exists():
            return str(exe)
        return str(vcpkg_dir / "vcpkg")
    return str(vcpkg_dir / "vcpkg")

def ensure_vcpkg_cloned(vcpkg_dir: Path, branch=None):
    if vcpkg_dir.exists():
        print(f"[INFO] vcpkg already present at: {vcpkg_dir}")
        return
    print(f"[INFO] Cloning vcpkg into: {vcpkg_dir}")
    run(["git", "clone", "https://github.com/microsoft/vcpkg.git", str(vcpkg_dir)])
    if branch:
        run(["git", "checkout", branch], cwd=str(vcpkg_dir))

def bootstrap_vcpkg(vcpkg_dir: Path, host_os: str):
    print("[INFO] Bootstrapping vcpkg")
    if host_os == "windows":
        script = str(vcpkg_dir / "bootstrap-vcpkg.bat")
        run([script])
    else:
        script = str(vcpkg_dir / "bootstrap-vcpkg.sh")
        run(["bash", script])

def is_tty():
    try:
        return sys.stdin.isatty()
    except Exception:
        return False

def prompt_choice(title, options, default_idx=0):
    """
    options: list of (key, label)
    returns key
    """
    print("\n" + title)
    for i, (_, label) in enumerate(options, start=1):
        d = " (default)" if (i - 1) == default_idx else ""
        print(f"  {i}) {label}{d}")
    while True:
        raw = input(f"Select [1-{len(options)}] (Enter=default): ").strip()
        if raw == "":
            return options[default_idx][0]
        if raw.isdigit():
            n = int(raw)
            if 1 <= n <= len(options):
                return options[n - 1][0]
        print("Invalid selection, try again.")

def prompt_yes_no(question, default=True):
    d = "Y/n" if default else "y/N"
    while True:
        raw = input(f"{question} [{d}]: ").strip().lower()
        if raw == "":
            return default
        if raw in ("y", "yes"):
            return True
        if raw in ("n", "no"):
            return False
        print("Please answer y/n.")

def install_packages(vcpkg_bin, packages, triplet, host_triplet=None, clean_after=True, overlay_triplets=None, env=None):
    if not packages:
        return
    cmd = [vcpkg_bin, "install", *packages, f"--triplet={triplet}"]
    if host_triplet:
        cmd.append(f"--host-triplet={host_triplet}")
    if clean_after:
        cmd.append("--clean-after-build")
    if overlay_triplets:
        cmd += ["--overlay-triplets", overlay_triplets]
    run(cmd, env=env)

def build_env(target_triplet=None, host_triplet=None):
    env = os.environ.copy()
    # You can still control default triplets via env if you like,
    # but we pass --triplet/--host-triplet explicitly for clarity.
    env["VCPKG_LIBRARY_LINKAGE"] = "static"
    if target_triplet:
        env["VCPKG_DEFAULT_TRIPLET"] = target_triplet
    if host_triplet:
        env["VCPKG_HOST_TRIPLET"] = host_triplet
    return env


# ----------------------------- triplet mapping -----------------------------

def default_host_triplet(host_os: str, host_arch: str):
    if host_os == "windows":
        if host_arch == "x64":
            return "x64-windows-static"
        if host_arch == "x86":
            return "x86-windows-static"
        if host_arch == "arm64":
            return "arm64-windows-static"
    if host_os == "linux":
        # vcpkg built-in: x64-linux, arm64-linux, etc. (usually static by default)
        return f"{host_arch}-linux" if host_arch in ("x64", "arm64") else "x64-linux"
    if host_os == "macos":
        # built-in: x64-osx / arm64-osx
        return f"{host_arch}-osx" if host_arch in ("x64", "arm64") else "x64-osx"
    return ""

def default_target_triplet_for_local(host_os: str, host_arch: str):
    # Default target = local desktop target
    return default_host_triplet(host_os, host_arch)

def target_triplet_options():
    # key is actual vcpkg triplet string
    return [
        # Desktop
        ("x64-windows-static", "Windows x64 (static)"),
        ("arm64-windows-static", "Windows arm64 (static)"),
        ("x64-linux",          "Linux x64"),
        ("arm64-linux",        "Linux arm64"),
        ("x64-osx",            "macOS x64 (Intel)"),
        ("arm64-osx",          "macOS arm64 (Apple Silicon)"),
        # Android (built-in & tested triplets)
        ("arm64-android",      "Android arm64-v8a (arm64-android)"),
        ("arm-neon-android",   "Android armeabi-v7a NEON (arm-neon-android)"),
        ("x64-android",        "Android x86_64 (x64-android)"),
        # iOS (triplets exist; simulator triplets may be community/overlay depending on your vcpkg version)
        ("arm64-ios",          "iOS Device arm64 (arm64-ios)"),
        ("arm64-ios-simulator","iOS Simulator arm64 (arm64-ios-simulator)"),
        ("x64-ios-simulator",  "iOS Simulator x64 (x64-ios-simulator)"),
    ]

def host_triplet_options():
    # Tools generally run on desktop hosts only
    return [
        ("x64-windows-static", "Host: Windows x64 (static)"),
        ("arm64-windows-static","Host: Windows arm64 (static)"),
        ("x64-linux",          "Host: Linux x64"),
        ("arm64-linux",        "Host: Linux arm64"),
        ("x64-osx",            "Host: macOS x64 (Intel)"),
        ("arm64-osx",          "Host: macOS arm64 (Apple Silicon)"),
    ]

def enforce_static_windows_triplet(role: str, triplet: str):
    blocked_triplets = {"x64-windows", "x86-windows", "arm64-windows"}
    if triplet in blocked_triplets:
        print(f"[ERROR] {role} triplet '{triplet}' is not allowed. Use a -windows-static triplet.", file=sys.stderr)
        sys.exit(2)


# ----------------------------- packages -----------------------------

HOST_TOOL_PACKAGES = [
    # shaderc/texturec/geometryc live in bgfx[tools]
    "bgfx[tools]",
]

# Runtime deps: keep bgfx WITHOUT tools so mobile is viable
RUNTIME_PACKAGES = [
    "bgfx[multithreaded]",
    "meshoptimizer",
    "draco",

    "openssl",

    "sdl3",
    "zlib",
    "minizip",

    "lua[cpp]",
    "angelscript[addons]",

    "bullet3[extras,multithreading]",

    "imgui",
    "rmlui[freetype,svg]",
    "implot",

    "boost-locale",
    "nlohmann-json",
    "pugixml",
    "glm",
    "tinyexr",
]


# ----------------------------- main -----------------------------

def parse_args():
    p = argparse.ArgumentParser(
        description="Orka Engine bootstrap via vcpkg (desktop + iOS/Android-ready)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=dedent("""
        What this script does
          1) Clones + bootstraps vcpkg (if needed)
          2) Installs HOST build-tools (bgfx[tools]) for shader compilation
          3) Installs TARGET runtime dependencies for your engine/app

        Mobile note
          - For iOS/Android, tools are installed for HOST (desktop), runtime for TARGET.
          - You compile shaders offline on the host, then ship the compiled shaders.

        Typical usage
          - Just run it (interactive):
              ./bootstrap.py
          - Non-interactive:
              ./bootstrap.py --target-triplet=arm64-android --host-triplet=x64-linux --clean-after-build

        """)
    )
    p.add_argument("--target-triplet", default="", help="vcpkg target triplet (where your app runs).")
    p.add_argument("--host-triplet", default="", help="vcpkg host triplet (where build-tools run).")
    p.add_argument("--clean-after-build", action="store_true", help="Pass --clean-after-build to vcpkg install.")
    p.add_argument("--no-clean-after-build", action="store_true", help="Do NOT pass --clean-after-build.")
    p.add_argument("--overlay-triplets", default="", help="Path to custom/community triplets, if any.")
    p.add_argument("--vcpkg-dir", default="vcpkg", help="vcpkg directory (default: ./vcpkg).")
    p.add_argument("--branch", default="", help="Checkout a specific vcpkg branch/tag.")
    p.add_argument("--rebootstrap", action="store_true", help="Force rebootstrap vcpkg.")
    p.add_argument("--no-install", action="store_true", help="Prepare vcpkg, skip installing packages.")
    p.add_argument("--no-tools", action="store_true", help="Skip installing HOST tools (bgfx[tools]).")
    p.add_argument("--no-runtime", action="store_true", help="Skip installing TARGET runtime packages.")
    p.add_argument("--non-interactive", action="store_true", help="Fail if required selections are missing.")
    return p.parse_args()

def main():
    args = parse_args()

    host_os = detect_os()
    host_arch = detect_arch()

    which_or_die("git", "Install Git: https://git-scm.com/downloads")
    if host_os != "windows":
        which_or_die("bash", "Install bash (usually present on Linux/macOS)")

    # clean-after-build preference
    if args.clean_after_build and args.no_clean_after_build:
        print("[WARN] Both clean flags set; using --clean-after-build.")
        clean_after = True
    elif args.clean_after_build:
        clean_after = True
    elif args.no_clean_after_build:
        clean_after = False
    else:
        # interactive default: yes; non-interactive default: yes (safe disk-wise)
        clean_after = True

    # Determine defaults
    default_host = default_host_triplet(host_os, host_arch)
    default_target = default_target_triplet_for_local(host_os, host_arch)

    # Interactive “guide”
    interactive = is_tty() and (not args.non_interactive)

    target_triplet = args.target_triplet.strip()
    host_triplet = args.host_triplet.strip()

    if interactive:
        print("[INFO] Orka Engine bootstrap (guided)")
        print(f"[INFO] Detected host: {host_os} / {host_arch}")
        print(f"[INFO] Suggested defaults: host={default_host} target={default_target}")

        # select target
        if not target_triplet:
            opts = target_triplet_options()
            # pick default index if present
            default_idx = 0
            for i, (k, _) in enumerate(opts):
                if k == default_target:
                    default_idx = i
                    break
            target_triplet = prompt_choice("Choose TARGET (where the app runs):", opts, default_idx)

        # select host
        if not host_triplet:
            opts = host_triplet_options()
            default_idx = 0
            for i, (k, _) in enumerate(opts):
                if k == default_host:
                    default_idx = i
                    break
            host_triplet = prompt_choice("Choose HOST (where build tools run):", opts, default_idx)

        # clean toggle
        clean_after = prompt_yes_no("Pass --clean-after-build to vcpkg?", default=clean_after)

    else:
        # non-interactive
        if not target_triplet:
            target_triplet = default_target
        if not host_triplet:
            host_triplet = default_host
        if not target_triplet or not host_triplet:
            print("[ERROR] Missing triplet selection. Provide --target-triplet and --host-triplet.", file=sys.stderr)
            sys.exit(2)

    enforce_static_windows_triplet("TARGET", target_triplet)
    enforce_static_windows_triplet("HOST", host_triplet)

    # Mobile sanity notes
    if "ios" in target_triplet:
        if host_os != "macos":
            print("[WARN] iOS target selected, but host OS is not macOS. iOS builds require macOS + Xcode.")
    if "android" in target_triplet:
        if not os.environ.get("ANDROID_NDK_HOME") and not os.environ.get("ANDROID_NDK_ROOT"):
            print("[HINT] Android target selected: consider setting ANDROID_NDK_HOME (or ANDROID_NDK_ROOT).")

    vcpkg_dir = Path(args.vcpkg_dir).resolve()
    branch = args.branch.strip() or None

    print("\n[PLAN]")
    print(f"  Host OS/Arch     : {host_os} / {host_arch}")
    print(f"  HOST triplet     : {host_triplet}")
    print(f"  TARGET triplet   : {target_triplet}")
    print("  Static linkage   : yes")
    print(f"  Clean after build: {'yes' if clean_after else 'no'}")
    if args.overlay_triplets:
        print(f"  Overlay triplets : {args.overlay_triplets}")

    # Clone & bootstrap vcpkg
    ensure_vcpkg_cloned(vcpkg_dir, branch=branch)
    vcpkg_bin = vcpkg_exe_path(vcpkg_dir, host_os)
    if args.rebootstrap or not Path(vcpkg_bin).exists():
        bootstrap_vcpkg(vcpkg_dir, host_os)

    env = build_env(target_triplet=target_triplet, host_triplet=host_triplet)

    if args.no_install:
        print("[INFO] Skipping installation (--no-install).")
        return

    # Install HOST tools (shaderc etc.)
    if not args.no_tools:
        print("\n[STEP] Installing HOST tools (shaderc etc.)")
        # Tools should be installed for the *host* triplet.
        # We still pass --host-triplet to be explicit when vcpkg needs host tools during builds.
        install_packages(
            vcpkg_bin=vcpkg_bin,
            packages=HOST_TOOL_PACKAGES,
            triplet=host_triplet,
            host_triplet=host_triplet,
            clean_after=clean_after,
            overlay_triplets=(args.overlay_triplets or None),
            env=env,
        )
    else:
        print("\n[SKIP] HOST tools (--no-tools)")

    # Install TARGET runtime dependencies
    if not args.no_runtime:
        print("\n[STEP] Installing TARGET runtime dependencies")
        install_packages(
            vcpkg_bin=vcpkg_bin,
            packages=RUNTIME_PACKAGES,
            triplet=target_triplet,
            host_triplet=host_triplet,
            clean_after=clean_after,
            overlay_triplets=(args.overlay_triplets or None),
            env=env,
        )
    else:
        print("\n[SKIP] TARGET runtime (--no-runtime)")

    print("\n[OK] vcpkg setup complete.")
    print("[HINT] CMake configure typically uses:")
    print(f"       -DCMAKE_TOOLCHAIN_FILE={vcpkg_dir}/scripts/buildsystems/vcpkg.cmake")
    print(f"       -DVCPKG_TARGET_TRIPLET={target_triplet}")
    print(f"       -DVCPKG_HOST_TRIPLET={host_triplet}")
    if "ios" in target_triplet:
        print("[HINT] For iOS: run on macOS, ensure Xcode + iOS SDK installed (xcode-select).")
    if "android" in target_triplet:
        print("[HINT] For Android: ensure NDK installed and ANDROID_NDK_HOME/ROOT set.")

if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as e:
        print(f"[ERROR] Command failed with exit code {e.returncode}", file=sys.stderr)
        sys.exit(e.returncode)
    except KeyboardInterrupt:
        print("\n[WARN] Aborted by user.", file=sys.stderr)
        sys.exit(130)
