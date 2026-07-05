#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description="Build PS2 Engine")
    parser.add_argument("build_type", nargs="?", default="debug", choices=["debug", "release"], help="Build type (debug or release)")
    parser.add_argument("region", nargs="?", default="pal", choices=["pal", "ntsc"], type=str.lower, help="Region (pal or ntsc)")
    
    args = parser.parse_args()
    
    debug_flag = "ON" if args.build_type == "debug" else "OFF"
    region_flag = args.region.upper()

    project_root = Path(__file__).resolve().parent.parent

    # Check if we are on Windows and not in WSL
    is_windows = sys.platform == "win32"
    is_wsl = "microsoft" in open("/proc/version", "r").read().lower() if os.path.exists("/proc/version") else False

    print("=== Initialising git submodules ===")
    
    # Submodule update
    if is_windows and not is_wsl:
        # On Windows, run in WSL
        wsl_distro = os.environ.get("PS2_WSL_DISTRO", "Ubuntu")
        print(f"=== Starting Build in WSL ({wsl_distro}) ===")
        
        # Git submodule in WSL
        subprocess.run(["wsl", "-d", wsl_distro, "bash", "-lc", "cd $(wslpath -u '{}') && git submodule update --init --recursive".format(project_root)], check=True)
        
        build_dir = f"build/{args.build_type}-{args.region.lower()}"
        print(f"=== Building engine & app ({args.build_type}, DEBUG={debug_flag}, REGION={region_flag}) ===")
        build_cmd = f"cd $(wslpath -u '{project_root}') && cmake -DCMAKE_TOOLCHAIN_FILE=ps2dev.cmake -DDEBUG='{debug_flag}' -DREGION='{region_flag}' -B {build_dir} && cmake --build {build_dir} --target generate-iso"
        
        try:
            subprocess.run(["wsl", "-d", wsl_distro, "bash", "-lc", build_cmd], check=True)
            print("=== Build Completed Successfully ===")
        except subprocess.CalledProcessError as e:
            print(f"=== Build Failed ===")
            sys.exit(e.returncode)
            
    else:
        # On Linux/Mac/WSL
        try:
            subprocess.run(["git", "submodule", "update", "--init", "--recursive"], cwd=project_root, check=True)
        except subprocess.CalledProcessError as e:
            sys.exit(e.returncode)
            
        print(f"=== Building engine & app ({args.build_type}, DEBUG={debug_flag}, REGION={region_flag}) ===")

        build_dir = f"build/{args.build_type}-{args.region.lower()}"

        cmake_configure = [
            "cmake",
            "-DCMAKE_TOOLCHAIN_FILE=ps2dev.cmake",
            f"-DDEBUG={debug_flag}",
            f"-DREGION={region_flag}",
            "-B", build_dir
        ]

        cmake_build = [
            "cmake",
            "--build", build_dir,
            "--target", "generate-iso"
        ]
        
        try:
            subprocess.run(cmake_configure, cwd=project_root, check=True)
            subprocess.run(cmake_build, cwd=project_root, check=True)
            print("=== Build Completed Successfully ===")
        except subprocess.CalledProcessError as e:
            print(f"=== Build Failed ===")
            sys.exit(e.returncode)

if __name__ == "__main__":
    main()
