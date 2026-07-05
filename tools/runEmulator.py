#!/usr/bin/env python3
import os
import subprocess
import sys
import shutil

def main():
    if len(sys.argv) > 1:
        file_path = sys.argv[1]
    else:
        file_path = "./dist/engine.iso"

    file_path = os.path.abspath(file_path)

    pcsx2_path = sys.argv[2] if len(sys.argv) > 2 else ""

    is_wsl = "microsoft" in open("/proc/version", "r").read().lower() if os.path.exists("/proc/version") else False
    is_mac = sys.platform == "darwin"
    is_windows = sys.platform == "win32"

    if not pcsx2_path:
        if is_mac:
            pcsx2_path = "/Applications/PCSX2.app/Contents/MacOS/PCSX2"
        elif is_wsl:
            paths = [
                "/c/PCSX2/pcsx2-qt.exe",
                "/mnt/c/PCSX2/pcsx2-qt.exe",
                "/c/Program Files/PCSX2/pcsx2-qt.exe",
                "/mnt/c/Program Files/PCSX2/pcsx2-qt.exe"
            ]
            for p in paths:
                if os.path.exists(p):
                    pcsx2_path = p
                    break
        elif is_windows:
            paths = [
                "C:\\PCSX2\\pcsx2-qt.exe",
                "C:\\Program Files\\PCSX2\\pcsx2-qt.exe"
            ]
            for p in paths:
                if os.path.exists(p):
                    pcsx2_path = p
                    break
        else:
            pcsx2_path = shutil.which("pcsx2-qt")
            if not pcsx2_path:
                pcsx2_path = shutil.which("pcsx2")

    if not pcsx2_path or not os.path.exists(pcsx2_path) and not shutil.which(pcsx2_path):
        print("Error: PCSX2 not found.")
        print("Usage: python3 tools/runEmulator.py [FilePath] [Pcsx2Path]")
        sys.exit(1)

    file_final = file_path
    if is_wsl:
        try:
            result = subprocess.run(["wslpath", "-w", file_path], capture_output=True, text=True, check=True)
            file_final = result.stdout.strip()
        except (subprocess.CalledProcessError, FileNotFoundError):
            pass

    print(f"=== Launching PCSX2 ({sys.platform}) ===")
    print(f"PCSX2: {pcsx2_path}")
    print(f"File:  {file_final}")

    # Launch PCSX2 detached
    cmd = [pcsx2_path]
    if not is_mac:
        cmd.append("-portable")
    cmd.extend(["-batch", file_final])

    if is_windows:
        subprocess.Popen(cmd, creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP)
    else:
        subprocess.Popen(cmd, start_new_session=True)

if __name__ == "__main__":
    main()
