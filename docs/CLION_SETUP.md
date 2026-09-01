# CLion Setup Guide

This guide configures CLion to build, run, and analyze the PS2 engine using the WSL toolchain.

---

## Prerequisites

| Requirement       | Details                                                                                          |
| :---------------- | :----------------------------------------------------------------------------------------------- |
| **CLion**         | 2023.1 or newer (CMakePresets.json v6 support required)                                          |
| **WSL2 + Ubuntu** | With the PS2 toolchain installed at `/usr/local/ps2dev` (see [PS2SDK_SETUP.md](ps2/PS2SDK_SETUP.md)) |
| **PCSX2**         | Installed on your platform (for the `run-emulator` target)                                       |
| **genisoimage**   | Installed in WSL (`sudo apt install genisoimage`) for ISO generation                             |
| **python3**       | Installed in WSL (`sudo apt install python3`) for the asset pipeline (`cook_assets.py`)               |

> `PS2DEV` does **not** need to be in your `~/.bashrc` for CLion — the default path `/usr/local/ps2dev` is hardcoded in `CMakePresets.json`. You only need it exported if you run builds from a terminal directly. If your toolchain is at a non-standard path, see the [Troubleshooting](#troubleshooting) section.

---

## 1. Configure the WSL Toolchain

1. Open **Settings** → **Build, Execution, Deployment** → **Toolchains**.
2. Click **+** → **WSL**.
3. Select your WSL distribution (e.g., `Ubuntu`).
4. CLion will auto-detect `cmake`, `make`, and `gdb` from WSL. Verify they appear in green.
5. Rename the entry to exactly **`WSL`** — the name must match the `"toolchain"` value in `CMakePresets.json`.
6. Move the WSL toolchain **to the top** of the list.

---

## 2. CMake Profiles (Code Analysis Only)

`CMakePresets.json` defines the profiles CLion loads automatically. Which
platforms a configure produces is chosen by `PLATFORMS_TO_SUPPORT`, which
defaults to every known platform and is filtered against the active toolchain.

| Preset             | DEBUG | PLATFORMS_TO_SUPPORT |
| :----------------- | :---- | :------------------- |
| `ps2-debug-pal`    | ON    | `PS2PAL`             |
| `ps2-debug-ntsc`   | ON    | `PS2NTSC`            |
| `ps2-debug`        | ON    | `PS2PAL;PS2NTSC`     |
| `ps2-release-pal`  | OFF   | `PS2PAL`             |
| `ps2-release-ntsc` | OFF   | `PS2NTSC`            |
| `ps2-release`      | OFF   | `PS2PAL;PS2NTSC`     |
| `win32-debug`      | ON    | `WIN32`              |
| `win32-release`    | OFF   | `WIN32`              |

Presets ending in `-linux` configure the same builds for a Linux host. The
retired `REGION` option no longer exists: broadcast region is now platform
identity, so `ps2pal` and `ps2ntsc` are separate platforms rather than one
platform with a switch. See [PLATFORMS.md](PLATFORMS.md).

All fields in these profiles (including the toolchain dropdown) are **grayed out** — this is expected CLion behavior for preset-based profiles. The toolchain is assigned via the `vendor` block in `CMakePresets.json`.

> **These profiles are for code analysis and IntelliSense only.** They do not drive the actual build. Use the CMake hammer with `main.elf` selected as the build target (see Section 3).

---

## 3. Building and Running

### Building with the CMake Hammer

`main.elf` is the build target for the engine. To build in CLion:

1. Select the desired CMake profile from the profile dropdown (e.g., `PS2 Debug (PAL)`).
2. In the **Build Target** dropdown next to the hammer (▲), select `main.elf`.
3. Click the hammer (▲) to build.

CMake handles the full pipeline automatically — ps2gl, ps2stuff, engine compilation, linking, and ISO generation all run in order via the dependency chain in `CMakeLists.txt`.

### Running the Emulator

A `run-emulator` CMake custom target is provided for launching PCSX2. It calls `tools/runEmulator.py` with the built ISO and works on both Windows (via WSL interop) and Linux natively.

To run from CLion:

1. In the **Build Target** dropdown, select `run-emulator`.
2. Click the hammer (▲).

See [tools/runEmulator.py](../tools/runEmulator.py) for supported PCSX2 install paths. You can also run it directly from a terminal:

```bash
python3 tools/runEmulator.py dist/ps2pal/engine.iso
# or pass the path explicitly:
python3 tools/runEmulator.py dist/ps2pal/engine.iso "C:/path/to/pcsx2-qt.exe"
```

---

## 4. Build Architecture

The CMake hammer invokes `cmake --build <binaryDir> --target main.elf`. `CMakeLists.txt` drives the full pipeline in order:

1. **Configure time**: generates `.clangd` for IDE analysis.
2. **Build time**: builds `ps2gl` → builds `ps2stuff` → compiles the engine → links `main.elf` → generates `dist/ps2pal/engine.iso`.

`ps2gl` and `ps2stuff` are only rebuilt if their `.a` files are missing (CMake output-based tracking). On incremental builds only the changed engine/app sources are recompiled.

`build.py` is also available as a thin CLI wrapper if you prefer building from a terminal:

```
python3 tools/build.py [debug|release] [pal|ntsc]
```

`build.py` targets a per-toolchain, per-config directory (`build/<toolchain>-<debug|release>`), matching the preset `binaryDir`s (`build/ps2dev-debug-pal`, `build/win32-debug`, and so on) that CLion loads for code analysis.

---

## 5. First-Time Workflow

1. Select the `PS2 Debug (PAL)` CMake profile and `main.elf` as the build target, then click the hammer (▲).
   - The full pipeline runs: ps2gl → engine compile → game compile → link → cook → package → distribution. See [PIPELINE.md](PIPELINE.md).
   - Output: `dist/ps2pal/main.elf` and `dist/ps2pal/engine.iso`.
2. **Reload CMake** in CLion after the first build: **Tools** → **CMake** → **Reload CMake Project**.
   - This picks up `build/compile_commands.json` and the generated `.clangd` so PS2SDK headers resolve correctly in the editor.
3. To run the emulator, select `run-emulator` as the build target and click the hammer (▲).

---

## Troubleshooting

### `PS2DEV environment variable is not set!`

The presets hardcode `/usr/local/ps2dev`. If your toolchain is at a different path, create `CMakeUserPresets.json` at the project root (it is gitignored) to override:

```json
{
    "version": 6,
    "configurePresets": [
        { "name": "ps2-debug-pal",    "environment": { "PS2DEV": "/your/path" } },
        { "name": "ps2-debug-ntsc",   "environment": { "PS2DEV": "/your/path" } },
        { "name": "ps2-release-pal",  "environment": { "PS2DEV": "/your/path" } },
        { "name": "ps2-release-ntsc", "environment": { "PS2DEV": "/your/path" } }
    ]
}
```

### Code analysis does not resolve PS2SDK headers

1. Confirm CLion ran CMake at least once for a profile (check the **Build** tab for configure output).
2. Ensure a successful `build.py` run has completed — `.clangd` is generated during the cmake configure step.
3. Reload: **Tools** → **CMake** → **Reload CMake Project**.
4. If the CMake output shows errors about missing compilers, verify the WSL toolchain name is exactly `WSL` (Step 1).

### CMake profiles are missing or show an error

1. CLion must be opened from the **project root** (`c:\dev\ps2-engine`), not a subdirectory.
2. Confirm the WSL toolchain is configured (Step 1) before CMake loads.
3. Try **File** → **Invalidate Caches** → **Invalidate and Restart**.

### PCSX2 not found by `runEmulator.py`

The script checks common install paths. Pass the path explicitly as the second argument:

```bash
python3 tools/runEmulator.py dist/ps2pal/engine.iso "C:/path/to/pcsx2-qt.exe"
```

### `genisoimage` / `mkisofs` not found (no ISO generated)

```bash
sudo apt install genisoimage
```

Then re-run the build — ISO generation runs as a POST_BUILD step automatically.
