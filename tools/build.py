#!/usr/bin/env python3
"""Build the engine and game for one or more platforms.

    python3 tools/build.py [debug|release] [pal|ntsc] [--platforms PS2PAL,PS2NTSC]

The positional region argument is kept for backwards compatibility: `pal` means
`--platforms PS2PAL`. With neither, every platform the active toolchain can
build is built, each into its own dist/<platform>/ bundle.
"""
import argparse
import os
import subprocess
import sys
from pathlib import Path

# Which toolchain file each platform needs. A configure uses one toolchain, so
# platforms are grouped by it and built one group at a time.
TOOLCHAINS = {
    "PS2PAL": "toolchains/ps2dev.cmake",
    "PS2NTSC": "toolchains/ps2dev.cmake",
    "WIN32": "toolchains/mingw-w64.cmake",
}

REGION_ALIAS = {"pal": "PS2PAL", "ntsc": "PS2NTSC"}


def parse_args():
    p = argparse.ArgumentParser(description="Build the PS2 engine")
    p.add_argument("build_type", nargs="?", default="debug", choices=["debug", "release"])
    p.add_argument("region", nargs="?", default=None, type=str.lower, choices=["pal", "ntsc"],
                   help="Shorthand for --platforms PS2PAL / PS2NTSC")
    p.add_argument("--platforms", default=None,
                   help="Comma-separated platforms (default: everything the toolchain supports)")
    return p.parse_args()


def resolve_platforms(args):
    if args.platforms:
        return [p.strip().upper() for p in args.platforms.split(",") if p.strip()]
    if args.region:
        return [REGION_ALIAS[args.region]]
    return []  # let CMake use its default list, filtered by the toolchain


def group_by_toolchain(platforms):
    """One configure per toolchain; unknown platforms are reported, not guessed."""
    if not platforms:
        return {TOOLCHAINS["PS2PAL"]: []}  # default list, PS2 toolchain

    groups = {}
    for p in platforms:
        if p not in TOOLCHAINS:
            sys.exit(f"Unknown platform '{p}'. Known: {', '.join(sorted(TOOLCHAINS))}")
        groups.setdefault(TOOLCHAINS[p], []).append(p)
    return groups


def run(cmd, cwd):
    print("+ " + " ".join(cmd))
    subprocess.run(cmd, cwd=cwd, check=True)


def build_native(root, args, groups):
    debug_flag = "ON" if args.build_type == "debug" else "OFF"

    for toolchain, platforms in groups.items():
        tag = Path(toolchain).stem.replace("-", "")
        build_dir = f"build/{tag}-{args.build_type}"

        configure = ["cmake", f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
                     f"-DDEBUG={debug_flag}", "-B", build_dir]
        if platforms:
            configure.append("-DPLATFORMS_TO_SUPPORT=" + ";".join(platforms))

        print(f"=== Configuring {platforms or 'all supported platforms'} ({toolchain}) ===")
        run(configure, root)
        print(f"=== Building ({args.build_type}) ===")
        run(["cmake", "--build", build_dir, "--target", "dist"], root)


def main():
    args = parse_args()
    root = Path(__file__).resolve().parent.parent
    groups = group_by_toolchain(resolve_platforms(args))

    is_windows = sys.platform == "win32"
    is_wsl = False
    if os.path.exists("/proc/version"):
        with open("/proc/version", "r") as f:
            is_wsl = "microsoft" in f.read().lower()

    print("=== Initialising git submodules ===")

    if is_windows and not is_wsl:
        # The PS2 toolchain and the MinGW cross-compiler both live in WSL, so a
        # Windows host re-invokes the whole build there - one code path for both.
        distro = os.environ.get("PS2_WSL_DISTRO", "Ubuntu")
        print(f"=== Starting build in WSL ({distro}) ===")

        forwarded = [args.build_type]
        if args.region:
            forwarded.append(args.region)
        if args.platforms:
            forwarded += ["--platforms", args.platforms]

        script = (
            f"cd $(wslpath -u '{root}') && "
            "git submodule update --init --recursive && "
            f"python3 ./tools/build.py {' '.join(forwarded)}"
        )
        try:
            subprocess.run(["wsl", "-d", distro, "bash", "-lc", script], check=True)
        except subprocess.CalledProcessError as e:
            print("=== Build Failed ===")
            sys.exit(e.returncode)
        print("=== Build Completed Successfully ===")
        return

    try:
        subprocess.run(["git", "submodule", "update", "--init", "--recursive"], cwd=root, check=True)
        build_native(root, args, groups)
    except subprocess.CalledProcessError as e:
        print("=== Build Failed ===")
        sys.exit(e.returncode)

    print("=== Build Completed Successfully ===")


if __name__ == "__main__":
    main()
