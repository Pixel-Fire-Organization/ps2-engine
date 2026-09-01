# PS2 Game Engine

A custom PlayStation 2 game engine written in C++, leveraging the official `ps2sdk` and `ps2gl` for native 2D/3D graphics, audio, input handling, and hardware acceleration on the PS2.

## Requirements

To reliably build this engine from source, you must have the PS2 toolchain properly installed and export its location to your environment.

1. **PS2 Toolchain:** 
   - Follow the official instructions to install the modern [`ps2dev` toolchain](https://github.com/ps2dev/ps2dev).
   - **Environment:** Ensure your shell exports the `PS2DEV` environment variable (e.g. `export PS2DEV=/usr/local/ps2dev`).
   - **IDE Setup (Automatic):** Simply running `python3 ./tools/build.py` (see below) will automatically generate a `.clangd` file that configures your editor's highlighting for both WSL and Windows.

2. **Dependencies:**
   - **CMake (3.10+)**
   - **genisoimage** (Provides the `mkisofs` utility required for automatically bundling bootable `.iso` files):
     ```bash
     # Ubuntu / Debian / WSL environments
     sudo apt-get update
     sudo apt-get install genisoimage
     ```

3. **Submodules (Third-Party Dependencies):**
   - The engine links statically with custom local compilations of `ps2gl` and `ps2stuff` located within the `external/` directory to ensure perfect compatibility.

## Build Instructions

We provide a convenient script in the `tools/` directory to effortlessly wipe old caches and cleanly rebuild the toolchain via CMake.

### Windows / WSL / Linux
`tools/build.py` detects Windows and automatically re-invokes itself inside WSL, so the same command works everywhere:

```bash
# Every platform the toolchain supports (PS2: both PAL and NTSC)
python3 ./tools/build.py [debug|release]

# One region only
python3 ./tools/build.py debug pal
python3 ./tools/build.py debug --platforms PS2NTSC
```

Or drive CMake directly. `PLATFORMS_TO_SUPPORT` defaults to every known platform and is
filtered against the toolchain, so it usually needs no flag:

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=toolchains/ps2dev.cmake -B build/ps2
cmake --build build/ps2 --target dist
```

## Running the Emulator

A script is provided to quickly launch the generated ISO in PCSX2.

```bash
python3 ./tools/runEmulator.py dist/ps2pal/engine.iso
```

## Build Artifacts

Each platform gets its own self-contained bundle under `dist/`, so builds never mix:

```
dist/ps2pal/    main.elf  engine.iso  main.sym     SYSTEM.CNF VMODE=PAL
dist/ps2ntsc/   main.elf  engine.iso  main.sym     SYSTEM.CNF VMODE=NTSC
dist/win32/     game.exe  RASSETS.PS2R  LEVELS/
```

- `main.elf` - the raw PS2 executable (handy for rapid testing over the network with `ps2client`).
- `engine.iso` - a self-bootable disc image for PCSX2 or real hardware.

### Custom ISO Assets

Any custom assets (textures, scripts, data files) that you want to include in the generated `.iso` should be placed in `game/cd_files/`. These files will be automatically bundled at the **root** of the ISO filesystem during the build process.

For example, a file at `game/cd_files/levels/map.bin` will be accessible on the PS2 as `cdrom0:\\LEVELS\\MAP.BIN;1`.

## Documentation

[docs/PLATFORMS.md](docs/PLATFORMS.md) indexes everything. Start there, or jump to:

| | |
|---|---|
| [Architecture](docs/ENGINE.md) | Layers, startup order, subsystems, the constants rule |
| [Guidelines](docs/guidelines/) | How to add a system or a platform — read before starting |
| [Game API](docs/APP_API.md) | The surface game code uses |
| [Build pipeline](docs/PIPELINE.md) | Compile, cook, package, distribute |
| [Authoring assets](docs/ASSET_AUTHORING.md) | Adding content |
| [PlayStation 2](docs/ps2/PLATFORM.md) | Platform spec, budgets, renderers |
| [Win32](docs/win32/PLATFORM.md) | Platform spec, budgets, renderers |

Specs carry the reasoning that is deliberately not in the source — hardware
quirks, race conditions, renderer limits, budget ceilings. Read the relevant one
before changing what it describes.
