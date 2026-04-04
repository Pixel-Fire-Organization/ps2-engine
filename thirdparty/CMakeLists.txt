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
target_compile_options(lua PRIVATE -w)

if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    # Derive PS2DEV/PS2SDK at configure time so build-phase commands have the correct env
    if(DEFINED ENV{PS2DEV})
        set(_PS2DEV "$ENV{PS2DEV}")
    else()
        set(_PS2DEV "/usr/local/ps2dev")
    endif()
    set(_PS2SDK "${_PS2DEV}/ps2sdk")
    set(_PS2_PATH "${_PS2DEV}/ee/bin:${_PS2DEV}/iop/bin:${_PS2DEV}/dvp/bin:$ENV{PATH}")

    # Patch raylib at configure time (idempotent — safe to run on every configure)
    execute_process(
        COMMAND bash "${CMAKE_SOURCE_DIR}/scripts/patch_raylib.sh"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE _PATCH_RESULT
    )
    if(NOT _PATCH_RESULT EQUAL 0)
        message(FATAL_ERROR "patch_raylib.sh failed")
    endif()

    # Build ps2gl if the static library does not yet exist
    add_custom_command(
        OUTPUT "${CMAKE_SOURCE_DIR}/thirdparty/ps2gl/libps2gl.a"
        COMMAND ${CMAKE_COMMAND} -E env "PS2DEV=${_PS2DEV}" "PS2SDK=${_PS2SDK}" "PATH=${_PS2_PATH}" ${CMAKE_MAKE_PROGRAM} "CFLAGS=-w"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/thirdparty/ps2gl"
        COMMENT "Building ps2gl..."
        VERBATIM
    )
    add_custom_target(ps2gl_build
        DEPENDS "${CMAKE_SOURCE_DIR}/thirdparty/ps2gl/libps2gl.a"
    )

    # Build raylib if the static library does not yet exist (patch applied above at configure time)
    add_custom_command(
        OUTPUT "${CMAKE_SOURCE_DIR}/thirdparty/raylib/src/libraylib.a"
        COMMAND ${CMAKE_COMMAND} -E env "PS2DEV=${_PS2DEV}" "PS2SDK=${_PS2SDK}" "PATH=${_PS2_PATH}" ${CMAKE_MAKE_PROGRAM} clean
        # WARNING_FLAGS=-w silences all warnings without overriding CFLAGS.
        # Passing CFLAGS=... on the command line would override the Makefile's
        # ifeq(PLATFORM_PLAYSTATION2) block which sets -DPLATFORM_PLAYSTATION2.
        COMMAND ${CMAKE_COMMAND} -E env "PS2DEV=${_PS2DEV}" "PS2SDK=${_PS2SDK}" "PATH=${_PS2_PATH}" ${CMAKE_MAKE_PROGRAM} PLATFORM=PLATFORM_PLAYSTATION2 "GRAPHICS=GRAPHICS_API_OPENGL_11 -I../../ps2gl/include" "WARNING_FLAGS=-w"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/thirdparty/raylib/src"
        DEPENDS "${CMAKE_SOURCE_DIR}/thirdparty/ps2gl/libps2gl.a"
        COMMENT "Building raylib for PS2..."
        VERBATIM
    )
    add_custom_target(raylib_build
        DEPENDS "${CMAKE_SOURCE_DIR}/thirdparty/raylib/src/libraylib.a"
    )
endif()
