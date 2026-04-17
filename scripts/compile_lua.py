#!/usr/bin/env python3
"""
compile_lua.py — PS2 Engine Lua Pre-compiler

Scans app/cd_files/ for .lua source files and compiles each one to a .LUC
bytecode file using the system's 'luac' tool.  Debug symbols are stripped
('-s' flag) to reduce the size of the bytecode.  The compiled .LUC files are
written alongside the source files and picked up by the ISO assembly pipeline
(copy_iso_files.cmake), while the .lua source files are excluded from the disc
image via the ISO content blacklist.

Usage:
    python3 scripts/compile_lua.py [--src <CD_FILES_DIR>] [--luac <LUAC_BIN>]
                                   [--max-size <BYTES>]

      --src      <dir>    Directory to scan for .lua files (default: app/cd_files/)
      --luac     <path>   Path or name of the luac binary (default: luac)
      --max-size <bytes>  Maximum allowed bytecode size per script in bytes.
                          Must match SCRIPTING_LUA_CODE_SLOT_SIZE in
                          engine/include/Constants.h (default: 262144 = 256 KB).
                          Scripts that exceed this limit cause the build to fail.

Compatibility note:
    The luac binary must produce bytecode compatible with the Lua version and
    word size embedded in the engine.  The PS2 EE processor is 32-bit
    little-endian; a 64-bit host luac produces bytecode with sizeof(size_t)=8,
    which the PS2 Lua VM will reject with "size_t size mismatch".  Use a
    32-bit luac build (or a cross-compiled luac) that matches the engine's Lua
    version to ensure the bytecode is accepted at runtime.

Exit codes:
    0   All scripts compiled successfully (or nothing to compile,
        or luac not found — pre-compilation is skipped with a warning).
    1   One or more scripts failed to compile or exceeded the slot size limit.
"""

import os
import shutil
import subprocess
import sys

# Output extension used for compiled Lua bytecode.
# Three characters to stay within ISO 9660 Level 1 extension limits and to
# match the SCRIPTING_MAIN_SCRIPT_PATH constant in engine/include/Constants.h.
COMPILED_EXT = ".LUC"

# Maximum bytecode size per script.  Must stay in sync with
# SCRIPTING_LUA_CODE_SLOT_SIZE in engine/include/Constants.h (256 KB).
# Each script occupies one dedicated arena code slot; exceeding this limit
# means the bytecode will not fit and the engine will reject the load at runtime.
DEFAULT_MAX_SIZE_BYTES = 256 * 1024


def find_lua_files(src_dir):
    """Return a sorted list of all .lua files (any case) found under src_dir."""
    lua_files = []
    for root, _, files in os.walk(src_dir):
        for name in files:
            if name.lower().endswith(".lua"):
                lua_files.append(os.path.join(root, name))
    return sorted(lua_files)


def compile_file(src_path, luac_bin, max_size_bytes):
    """Compile a single .lua file to a .LUC bytecode file.

    Passes '-s' to luac to strip debug information (line numbers, local
    variable names), reducing the bytecode size without affecting runtime
    behaviour.

    After compilation the output size is checked against max_size_bytes (which
    must match SCRIPTING_LUA_CODE_SLOT_SIZE in engine/include/Constants.h).
    Scripts that exceed the limit fail the build so that the engine never
    receives a bytecode blob that cannot fit into its arena code slot.

    Returns True on success, False on failure.
    """
    base, _ = os.path.splitext(src_path)
    out_path = base + COMPILED_EXT

    try:
        result = subprocess.run(
            [luac_bin, "-s", "-o", out_path, src_path],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            err = (result.stderr or result.stdout).strip()
            print(f"  ERROR: {os.path.basename(src_path)}: {err}")
            return False

        src_size = os.path.getsize(src_path)
        out_size = os.path.getsize(out_path)

        if out_size > max_size_bytes:
            print(
                f"  ERROR: {os.path.basename(out_path)} is too large for a code slot "
                f"({out_size} B > {max_size_bytes} B limit). "
                f"Reduce script size or raise SCRIPTING_LUA_CODE_SLOT_SIZE in "
                f"engine/include/Constants.h."
            )
            os.remove(out_path)
            return False

        print(
            f"  OK:   {os.path.basename(src_path)} -> "
            f"{os.path.basename(out_path)} "
            f"({src_size} B -> {out_size} B, limit {max_size_bytes} B)"
        )
        return True
    except OSError as exc:
        print(f"  ERROR: {os.path.basename(src_path)}: {exc}")
        return False


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)

    src_dir = os.path.join(project_root, "app", "cd_files")
    luac_bin = "luac"
    max_size_bytes = DEFAULT_MAX_SIZE_BYTES

    args = sys.argv[1:]
    argIndex = 0
    while argIndex < len(args):
        if args[argIndex] == "--src" and argIndex + 1 < len(args):
            src_dir = args[argIndex + 1]
            argIndex += 2
        elif args[argIndex] == "--luac" and argIndex + 1 < len(args):
            luac_bin = args[argIndex + 1]
            argIndex += 2
        elif args[argIndex] == "--max-size" and argIndex + 1 < len(args):
            try:
                max_size_bytes = int(args[argIndex + 1])
            except ValueError:
                print(f"ERROR: --max-size value must be an integer, got '{args[argIndex + 1]}'")
                return 1
            argIndex += 2
        else:
            argIndex += 1

    luac_resolved = shutil.which(luac_bin)
    if luac_resolved is None:
        print(
            f"WARNING: '{luac_bin}' not found — Lua pre-compilation skipped.\n"
            "         Install the Lua toolchain (provides luac) to enable pre-compilation.\n"
            "         The engine requires .LUC bytecode on disc; builds will fail at runtime without it.\n"
            "         See docs/ISO_GENERATION.md for details."
        )
        return 0

    if not os.path.isdir(src_dir):
        print(f"Source directory not found: {src_dir}")
        print("Nothing to compile.")
        return 0

    lua_files = find_lua_files(src_dir)
    if not lua_files:
        print(f"No .lua files found in {src_dir}")
        print("Nothing to compile.")
        return 0

    print(
        f"Compiling {len(lua_files)} Lua script(s) from "
        f"{src_dir} using {luac_resolved} (slot limit: {max_size_bytes} B)"
    )
    errors = 0
    for src_path in lua_files:
        if not compile_file(src_path, luac_resolved, max_size_bytes):
            errors += 1

    if errors > 0:
        print(f"\nFinished with {errors} error(s).")
        return 1

    print(f"\nAll {len(lua_files)} script(s) compiled successfully.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
