# PS2 Game Engine

A custom PlayStation 2 game engine written in C, leveraging the official `ps2sdk` and a local `raylib4PlayStation2` framework for native 2D/3D graphics, audio, input handling, and hardware acceleration on the PS2.

## Requirements

To reliably build this engine from source, you must have the PS2 toolchain properly installed and export its location to your environment.

1. **PS2 Toolchain:** 
   - Follow the official instructions to install the modern [`ps2dev` toolchain](https://github.com/ps2dev/ps2dev).
   - Ensure your shell exports the `PS2DEV` environment variable correctly (e.g. `export PS2DEV=/usr/local/ps2dev`).

2. **Dependencies:**
   - **CMake (3.10+)**
   - **genisoimage** (Provides the `mkisofs` utility required for automatically bundling bootable `.iso` files):
     ```bash
     # Ubuntu / Debian / WSL environments
     sudo apt-get update
     sudo apt-get install genisoimage
     ```

3. **Submodules (Third-Party Dependencies):**
   - The engine links statically with custom local compilations of `raylib` and `ps2gl` located within the `thirdparty/` directory to ensure perfect compatibility.

## Build Instructions

We provide a convenient bash script (`build.sh`) to effortlessly wipe old caches and cleanly rebuild the toolchain via CMake perfectly mapped to the PS2's `mips64r5900el` architecture.
To execute the build script, please use **WSL (preferred)** or **Git Bash** on Windows. 

```bash
chmod +x build.sh
./build.sh
```

Alternatively, you can execute the CMake generation sequence manually:

```bash
# 1. Generates the Makefile build cache targeting the custom toolchain
cmake -DCMAKE_TOOLCHAIN_FILE=ps2dev.cmake -B build

# 2. Compiles the static library and test application
cmake --build build
```

## Build Artifacts

All successfully linked targets are automatically routed away from the build sludge into the dedicated `exec/` directory located at the root of the project workspace.

- `exec/engine_test.elf` - The raw, unpacked PS2 executable (Recommended for rapid testing over network using `ps2client`).
- `exec/engine_test.iso` - A completely bundled, self-bootable disk image ready for PCSX2 or mounting on authentic hardware (relies on `SYSTEM.CNF`).
