# CLion Setup Guide

This guide configures CLion to build, run, and analyze the PS2 engine using the WSL toolchain.

---

## Prerequisites

| Requirement       | Details                                                                                          |
| :---------------- | :----------------------------------------------------------------------------------------------- |
| **CLion**         | 2023.1 or newer (CMakePresets.json v6 support required)                                          |
| **WSL2 + Ubuntu** | With the PS2 toolchain installed at `/usr/local/ps2dev` (see [PS2SDK_SETUP.md](PS2SDK_SETUP.md)) |
| **PCSX2**         | Installed on your platform (for the `run-emulator` target)                                       |
| **genisoimage**   | Installed in WSL (`sudo apt install genisoimage`) for ISO generation                             |

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

`CMakePresets.json` defines four profiles that CLion loads automatically:

| Profile              | binaryDir            | DEBUG | REGION |
| :------------------- | :------------------- | :---- | :----- |
| `PS2 Debug (PAL)`    | `build/debug-pal`    | ON    | PAL    |
| `PS2 Debug (NTSC)`   | `build/debug-ntsc`   | ON    | NTSC   |
| `PS2 Release (PAL)`  | `build/release-pal`  | OFF   | PAL    |
| `PS2 Release (NTSC)` | `build/release-ntsc` | OFF   | NTSC   |

All fields in these profiles (including the toolchain dropdown) are **grayed out** — this is expected CLion behavior for preset-based profiles. The toolchain is assigned via the `vendor` block in `CMakePresets.json`.

> **These profiles are for code analysis and IntelliSense only.** They do not drive the actual build. Use the CMake hammer with `main.elf` selected as the build target (see Section 3).

---

## 3. Building and Running

### Building with the CMake Hammer

`main.elf` is the build target for the engine. To build in CLion:

1. Select the desired CMake profile from the profile dropdown (e.g., `PS2 Debug (PAL)`).
2. In the **Build Target** dropdown next to the hammer (▲), select `main.elf`.
3. Click the hammer (▲) to build.

CMake handles the full pipeline automatically — ps2gl, raylib, engine compilation, linking, and ISO generation all run in order via the dependency chain in `CMakeLists.txt`.

### Running the Emulator

A `run-emulator` CMake custom target is provided for launching PCSX2. It calls `scripts/runEmulator.sh` with the built ISO and works on both Windows (via WSL interop) and Linux natively.

To run from CLion:

1. In the **Build Target** dropdown, select `run-emulator`.
2. Click the hammer (▲).

See [scripts/runEmulator.sh](../scripts/runEmulator.sh) for supported PCSX2 install paths. You can also run it directly from a terminal:

```bash
bash scripts/runEmulator.sh exec/engine.iso
# or pass the path explicitly:
bash scripts/runEmulator.sh exec/engine.iso "C:/path/to/pcsx2-qt.exe"
```

---

## 4. Build Architecture

The CMake hammer invokes `cmake --build <binaryDir> --target main.elf`. `CMakeLists.txt` drives the full pipeline in order:

1. **Configure time**: patches raylib (idempotent), generates `.clangd` for IDE analysis.
2. **Build time**: builds `ps2gl` → builds `raylib` → compiles the engine → links `main.elf` → generates `exec/engine.iso`.

`ps2gl` and `raylib` are only rebuilt if their `.a` files are missing (CMake output-based tracking). On incremental builds only the changed engine/app sources are recompiled.

`build.sh` is also available as a thin CLI wrapper if you prefer building from a terminal:

```
bash scripts/build.sh [debug|release] [pal|ntsc]
```

Note that `build.sh` always targets the single `build/` directory regardless of the active CLion profile. The CMake profile `binaryDir`s (`build/debug-pal`, etc.) are separate directories used only by CLion for code analysis.

---

## 5. First-Time Workflow

1. Select the `PS2 Debug (PAL)` CMake profile and `main.elf` as the build target, then click the hammer (▲).
   - The full pipeline runs: ps2gl → raylib → engine compile → link → ISO generation.
   - Output: `exec/main.elf` and `exec/engine.iso`.
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
2. Ensure a successful `build.sh` run has completed — `.clangd` is generated during the cmake configure step.
3. Reload: **Tools** → **CMake** → **Reload CMake Project**.
4. If the CMake output shows errors about missing compilers, verify the WSL toolchain name is exactly `WSL` (Step 1).

### CMake profiles are missing or show an error

1. CLion must be opened from the **project root** (`c:\dev\ps2-engine`), not a subdirectory.
2. Confirm the WSL toolchain is configured (Step 1) before CMake loads.
3. Try **File** → **Invalidate Caches** → **Invalidate and Restart**.

### PCSX2 not found by `runEmulator.sh`

The script checks common install paths. Pass the path explicitly as the second argument:

```bash
bash scripts/runEmulator.sh exec/engine.iso "C:/path/to/pcsx2-qt.exe"
```

### `genisoimage` / `mkisofs` not found (no ISO generated)

```bash
sudo apt install genisoimage
```

Then re-run the build — ISO generation runs as a POST_BUILD step automatically.
