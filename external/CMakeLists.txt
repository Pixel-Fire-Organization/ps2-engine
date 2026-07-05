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
    find_program(PYTHON3_BIN python3)
    if(NOT PYTHON3_BIN)
        set(PYTHON3_BIN "python3")
    endif()

    # Derive PS2DEV/PS2SDK at configure time so build-phase commands have the correct env
    if(DEFINED ENV{PS2DEV})
        set(_PS2DEV "$ENV{PS2DEV}")
    else()
        set(_PS2DEV "/usr/local/ps2dev")
    endif()
    set(_PS2SDK "${_PS2DEV}/ps2sdk")
    set(_PS2_PATH "${_PS2DEV}/ee/bin:${_PS2DEV}/iop/bin:${_PS2DEV}/dvp/bin:$ENV{PATH}")

    # Patch luaconf.h at configure time (idempotent — safe to run on every configure).
    # Sets LUA_32BITS=1 so lua_Number→float and lua_Integer→int, mapping all Lua
    # arithmetic to the PS2 EE COP1 hardware FPU (single-precision only; double is
    # software-emulated and ~5-10x slower).  CMake dependency tracking automatically
    # detects the header change and recompiles liblua.a without manual intervention.
    execute_process(
            COMMAND ${PYTHON3_BIN} "${CMAKE_SOURCE_DIR}/tools/patch_lua.py"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESULT_VARIABLE _LUA_PATCH_RESULT
    )
    if (NOT _LUA_PATCH_RESULT EQUAL 0)
        message(FATAL_ERROR "patch_lua.py failed")
    endif ()

    # Stamp file lives inside the CMake build tree so `cmake --build --target clean`
    # removes it and forces a rebuild next time.  Using a stamp (rather than the
    # installed .a path) means CMake emits exactly ONE rule for ps2stuff regardless
    # of how many other targets depend on it — eliminating the parallel-build race
    # where two make jobs both try to `cp` the .a and one fails with "File exists".
    set(_PS2STUFF_STAMP "${CMAKE_BINARY_DIR}/ps2stuff.installed.stamp")

    # Build and install ps2stuff first; its install target copies ps2s/ headers to
    # $(PS2SDK)/ports/include/ps2s/ which ps2gl requires at compile time.
    add_custom_command(
        OUTPUT "${_PS2STUFF_STAMP}"
        COMMAND ${CMAKE_COMMAND} -E env "PS2DEV=${_PS2DEV}" "PS2SDK=${_PS2SDK}" "PATH=${_PS2_PATH}" ${CMAKE_MAKE_PROGRAM}
        COMMAND ${CMAKE_COMMAND} -E rm -f "${_PS2SDK}/ports/lib/libps2stuff.a"
        COMMAND ${CMAKE_COMMAND} -E env "PS2DEV=${_PS2DEV}" "PS2SDK=${_PS2SDK}" "PATH=${_PS2_PATH}" ${CMAKE_MAKE_PROGRAM} install
        COMMAND ${CMAKE_COMMAND} -E touch "${_PS2STUFF_STAMP}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/external/ps2stuff"
        COMMENT "Building and installing ps2stuff..."
        VERBATIM
    )
    add_custom_target(ps2stuff_build
        DEPENDS "${_PS2STUFF_STAMP}"
    )

    # Build ps2gl after ps2stuff has installed ps2s/ headers into $(PS2SDK)/ports/include.
    # ps2gl's develop branch is its own standalone CMake project (no Makefile),
    # so it's configured and built out-of-tree into its own build dir under ours,
    # then linked directly from there.
    #
    # Deliberately NOT passing our own ps2dev.cmake toolchain here: it forces
    # -std=c++11, but ps2stuff's ps2s/core.h (which ps2gl includes) uses
    # std::align_val_t (C++17 aligned new) and fails to compile under that flag.
    # Omitting CMAKE_TOOLCHAIN_FILE lets ps2gl fall back to its own default
    # ($PS2DEV/share/ps2dev.cmake, installed by ps2dev itself), which leaves the
    # compiler's own default C++ standard (new enough) untouched.
    set(PS2GL_BUILD_DIR "${CMAKE_BINARY_DIR}/ps2gl_build")
    set(PS2GL_LIB "${PS2GL_BUILD_DIR}/libps2gl.a" CACHE INTERNAL "Path to the built ps2gl static library")

    add_custom_command(
        OUTPUT "${PS2GL_LIB}"
        COMMAND ${CMAKE_COMMAND} -E env "PS2DEV=${_PS2DEV}" "PS2SDK=${_PS2SDK}" "PATH=${_PS2_PATH}"
            ${CMAKE_COMMAND} -S "${CMAKE_SOURCE_DIR}/external/ps2gl" -B "${PS2GL_BUILD_DIR}"
            -DBUILD_TESTS=OFF -DBUILD_GLUT=OFF -DBUILD_EXAMPLES=OFF
        COMMAND ${CMAKE_COMMAND} -E env "PS2DEV=${_PS2DEV}" "PS2SDK=${_PS2SDK}" "PATH=${_PS2_PATH}"
            ${CMAKE_COMMAND} --build "${PS2GL_BUILD_DIR}" --target ps2gl
        DEPENDS "${_PS2STUFF_STAMP}"
        COMMENT "Configuring and building ps2gl (CMake)..."
        VERBATIM
    )
    add_custom_target(ps2gl_build
        DEPENDS "${PS2GL_LIB}"
    )
    add_dependencies(ps2gl_build ps2stuff_build)
endif()
