#!/usr/bin/bash

# patch_lua.sh
# Patches Lua 5.4's luaconf.h for PS2 hardware compatibility.
# Patches are idempotent — safe to re-run on every CMake configure.
#
# Applied patches:
#   [1] LUA_PATCHED_32BITS — sets LUA_32BITS=1 so lua_Number maps to float
#                            and lua_Integer maps to int.
#                            The PS2 EE COP1 FPU is single-precision only;
#                            double is software-emulated and ~5–10x slower.

LUACONF_FILE="thirdparty/lua/luaconf.h"

# Verify the file exists (submodule must be checked out)
if [[ ! -f "$LUACONF_FILE" ]]; then
    echo "Error: Could not find $LUACONF_FILE — is the lua submodule initialised?"
    exit 1
fi

PATCHED=0

# ---------------------------------------------------------------------------
# Patch 1: LUA_32BITS = 1  (lua_Number → float, lua_Integer → int)
#
# The PS2 EE's COP1 floating-point unit supports only single-precision
# IEEE 754 arithmetic (add.s, mul.s, div.s, sqrt.s …).  It has NO hardware
# double-precision instructions.  With the Lua 5.4 default (LUA_32BITS=0),
# lua_Number is 'double' and every arithmetic operation — including every
# math.sin / math.cos — goes through ps2sdk's software double emulation,
# which is 5–10× slower than the hardware path.
#
# Setting LUA_32BITS=1 switches:
#   lua_Number  →  float   (hardware FPU, full IEEE 754 single precision)
#   lua_Integer →  int     (native 32-bit EE integer unit)
#   math.*      →  sinf, cosf, sqrtf … (via l_mathop macro in luaconf.h)
#
# All integer handles used by this engine (resource ids, camera slots, file
# descriptors, frame counters) fit well within a 32-bit signed integer.
# ---------------------------------------------------------------------------
if grep -q "LUA_PATCHED_32BITS" "$LUACONF_FILE"; then
    echo "[1/1] LUA_32BITS patch already applied — skipping."
else
    echo "[1/1] Patching luaconf.h: LUA_32BITS=1 for PS2 hardware FPU..."

    sed -i 's|#define LUA_32BITS\t0|#define LUA_32BITS\t1 /* LUA_PATCHED_32BITS: PS2 EE COP1 FPU is single-precision only */|g' "$LUACONF_FILE"

    if grep -q "LUA_PATCHED_32BITS" "$LUACONF_FILE"; then
        echo "[1/1] LUA_32BITS patch applied."
        PATCHED=1
    else
        echo "Error: sed substitution did not produce the expected result in $LUACONF_FILE"
        exit 1
    fi
fi

# CMake tracks luaconf.h as a compiler-scanned dependency of every Lua .c file.
# When this script modifies it, the next 'cmake --build' automatically recompiles
# all Lua sources — no manual cache deletion is needed (unlike raylib which is
# built by an external Makefile CMake cannot track).
if [[ $PATCHED -eq 1 ]]; then
    echo "Lua patch applied. liblua.a will be rebuilt automatically by CMake."
else
    echo "All Lua patches already applied — nothing to do."
fi

exit 0

