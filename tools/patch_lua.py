#!/usr/bin/env python3
import os
import sys

def main():
    luaconf_file = os.path.join("external", "lua", "luaconf.h")

    if not os.path.isfile(luaconf_file):
        print(f"Error: Could not find {luaconf_file} — is the lua submodule initialised?")
        sys.exit(1)

    with open(luaconf_file, "r") as f:
        content = f.read()

    if "LUA_PATCHED_32BITS" in content:
        print("[1/1] LUA_32BITS patch already applied — skipping.")
        sys.exit(0)

    print("[1/1] Patching luaconf.h: LUA_32BITS=1 for PS2 hardware FPU...")
    
    # Replace the #define LUA_32BITS 0 line
    # Note: Using .replace() for safe replacement. Original file might use spaces or tabs.
    new_content = content.replace(
        "#define LUA_32BITS\t0",
        "#define LUA_32BITS\t1 /* LUA_PATCHED_32BITS: PS2 EE COP1 FPU is single-precision only */"
    )
    new_content = new_content.replace(
        "#define LUA_32BITS 0",
        "#define LUA_32BITS\t1 /* LUA_PATCHED_32BITS: PS2 EE COP1 FPU is single-precision only */"
    )

    if "LUA_PATCHED_32BITS" not in new_content:
        print(f"Error: substitution did not produce the expected result in {luaconf_file}")
        sys.exit(1)

    with open(luaconf_file, "w") as f:
        f.write(new_content)

    print("Lua patch applied. liblua.a will be rebuilt automatically by CMake.")

if __name__ == "__main__":
    main()
