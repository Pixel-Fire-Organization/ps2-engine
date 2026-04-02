# Thirdparty libraries CMake configuration
cmake_minimum_required(VERSION 3.10)
project(lua C)

# Define the Lua library
# This is placed here to avoid modifying the submodule's directory structure
set(LUA_SOURCES
    lua/lapi.c lua/lauxlib.c lua/lbaselib.c lua/lcode.c lua/lcorolib.c lua/lctype.c
    lua/ldblib.c lua/ldebug.c lua/ldo.c lua/ldump.c lua/lfunc.c lua/lgc.c lua/linit.c
    lua/liolib.c lua/llex.c lua/lmathlib.c lua/lmem.c lua/loadlib.c lua/lobject.c
    lua/lopcodes.c lua/loslib.c lua/lparser.c lua/lstate.c lua/lstring.c lua/lstrlib.c
    lua/ltable.c lua/ltablib.c lua/ltm.c lua/lundump.c lua/lutf8lib.c lua/lvm.c lua/lzio.c
)

add_library(lua STATIC ${LUA_SOURCES})

# Disable OS-specific features not available or buggy on PS2
target_compile_definitions(lua PRIVATE LUA_USE_C89)
target_include_directories(lua PUBLIC lua)
