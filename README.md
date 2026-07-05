# PS2 Game Engine

A custom PlayStation 2 game engine written in C, leveraging the official `ps2sdk` and a local `raylib4PlayStation2` framework for native 2D/3D graphics, audio, input handling, and hardware acceleration on the PS2.

## Requirements

To reliably build this engine from source, you must have the PS2 toolchain properly installed and export its location to your environment.

1. **PS2 Toolchain:** 
   - Follow the official instructions to install the modern [`ps2dev` toolchain](https://github.com/ps2dev/ps2dev).
   - **Environment:** Ensure your shell exports the `PS2DEV` environment variable (e.g. `export PS2DEV=/usr/local/ps2dev`).
   - **IDE Setup (Automatic):** Simply running `./tools/build.sh` (see below) will automatically generate a `.clangd` file that configures your editor's highlighting for both WSL and Windows.

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

We provide convenient scripts in the `tools/` directory to effortlessly wipe old caches and cleanly rebuild the toolchain via CMake.

### Windows (Recommended)
You can use the PowerShell or Batch wrappers which automatically handle WSL invocation:

```powershell
# From the project root:
./tools/build.ps1
```

Or using the batch file:
```cmd
.\scripts\build.bat
```

### WSL / Linux
To execute the bash build script directly:

```bash
chmod +x tools/build.sh
./tools/build.sh
```

Alternatively, you can execute the CMake generation sequence manually:

```bash
# 1. Generates the Makefile build cache targeting the custom toolchain
cmake -DCMAKE_TOOLCHAIN_FILE=ps2dev.cmake -B build

# 2. Compiles the static library and test application
cmake --build build
```

## Running the Emulator

A script is provided to quickly launch the generated ISO in PCSX2.

### Windows
```powershell
./tools/runEmulator.ps1
```

### WSL / Linux (cross-calling Windows PCSX2)
```bash
./tools/runEmulator.sh
```

## Build Artifacts

All successfully linked targets are automatically routed away from the build sludge into the dedicated `dist/` directory located at the root of the project workspace.

- `dist/main.elf` - The raw, unpacked PS2 executable (Recommended for rapid testing over network using `ps2client`).
- `dist/engine.iso` - A completely bundled, self-bootable disk image ready for PCSX2 or mounting on authentic hardware (relies on `SYSTEM.CNF`).

### Custom ISO Assets

Any custom assets (textures, scripts, data files) that you want to include in the generated `.iso` should be placed in `game/cd_files/`. These files will be automatically bundled at the **root** of the ISO filesystem during the build process.

For example, a file at `game/cd_files/levels/map.bin` will be accessible on the PS2 as `cdrom0:\\LEVELS\\MAP.BIN;1`.
