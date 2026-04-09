cmake_minimum_required(VERSION 3.10)
project(ps2_engine C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DEBUG "Enable debug features" ON)
if(DEBUG)
    add_definitions(-DDEBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif()

set(REGION "PAL" CACHE STRING "Target region (PAL or NTSC)")
set_property(CACHE REGION PROPERTY STRINGS "PAL" "NTSC")

if(NOT REGION STREQUAL "PAL" AND NOT REGION STREQUAL "NTSC")
    message(FATAL_ERROR "REGION must be PAL or NTSC, but found: '${REGION}'")
endif()
string(STRIP "${REGION}" REGION)
message(STATUS "CHECK: Current Region set to: ${REGION}")

if(REGION STREQUAL "PAL")
    add_definitions(-DREGION_PAL=1)
else()
    add_definitions(-DREGION_NTSC=1)
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic")
    message(WARNING "You should build this project using the PS2 toolchain file. E.g.: cmake -DCMAKE_TOOLCHAIN_FILE=ps2dev.cmake -B build")
endif()

set(APP_EXE_NAME "main.elf")
set(APP_ISO_NAME "engine.iso")
set(APP_REGION ${REGION})
set(APP_SERIAL ${APP_EXE_NAME})

option(USERETAILNAME "Changes the executable's name to the retail scheme." OFF)
if(USERETAILNAME)

# PS2 game serial — used as the boot executable name inside the ISO and as the
# disc volume label.  OPL2 Manager (and OPL itself) parse the BOOT2 field of
# SYSTEM.CNF to extract a serial; a plain "MAIN.ELF" name fails that parse.
# Format: XXYY_NNN.NN  (e.g. SLES_508.77 for retail, SLHB_000.00 for homebrew).
# Override with -DAPP_SERIAL=... when configuring for a distinct release.
set(APP_SERIAL "SLHB_000.00" CACHE STRING "PS2 game serial placed inside the ISO (format XXYY_NNN.NN)")
string(TOUPPER "${APP_SERIAL}" APP_SERIAL)
# Derive the volume-label form: SLHB_000.00 -> SLHB-00000
string(REPLACE "." "" _APP_SERIAL_NODOT "${APP_SERIAL}")
string(REPLACE "_" "-" APP_SERIAL_LABEL "${_APP_SERIAL_NODOT}")

endif()

# PS2 cdvdman (IOP) performs case-sensitive UPPERCASE lookups in the ISO 9660
# directory.  mkisofs -l preserves the case it is given, so a lowercase serial
# (e.g. "main.elf") produces a lowercase directory entry that cdvdman cannot
# find, causing "open fail" and a BIOS crash loop.  Always normalise APP_SERIAL
# to uppercase here so SYSTEM.CNF and the ISO directory entry always match.
# When USERETAILNAME=ON the serial is already uppercase; this is idempotent.
string(TOUPPER "${APP_SERIAL}" APP_SERIAL)

# Ensure APP_SERIAL_LABEL is always defined.  When USERETAILNAME=ON it is
# derived from the retail serial above.  When OFF derive a safe default from
# APP_ISO_NAME (e.g. "engine.iso" -> "ENGINE") so mkisofs -V never receives an
# empty string, which would produce an invalid ISO volume label.
if(NOT DEFINED APP_SERIAL_LABEL)
    string(REPLACE ".iso" "" _APP_ISO_STEM "${APP_ISO_NAME}")
    string(TOUPPER "${_APP_ISO_STEM}" APP_SERIAL_LABEL)
endif()

file(WRITE "${CMAKE_BINARY_DIR}/SYSTEM.CNF" "BOOT2 = cdrom0:\\${APP_SERIAL};1\r\nVER = 1.00\r\nVMODE = ${APP_REGION}\r\n")

add_subdirectory(thirdparty)
add_subdirectory(engine)
add_subdirectory(app)

if(CMAKE_SYSTEM_NAME STREQUAL "Generic")

    add_custom_target(run-emulator
        COMMAND bash "${CMAKE_SOURCE_DIR}/scripts/runEmulator.sh" "${CMAKE_SOURCE_DIR}/exec/engine.iso"
        COMMENT "Launching PCSX2 with ISO..."
        VERBATIM
    )

    add_custom_target(run-emulator-elf
        COMMAND bash "${CMAKE_SOURCE_DIR}/scripts/runEmulator.sh" "${CMAKE_SOURCE_DIR}/exec/${APP_EXE_NAME}"
        COMMENT "Launching PCSX2 with ELF..."
        VERBATIM
    )

    # Derive toolchain paths (mirrors thirdparty/CMakeLists.txt so the thirdparty
    # Makefiles can find the PS2 cross-compiler during the clean step).
    if(DEFINED ENV{PS2DEV})
        set(_CLEAN_PS2DEV "$ENV{PS2DEV}")
    else()
        set(_CLEAN_PS2DEV "/usr/local/ps2dev")
    endif()
    set(_CLEAN_PS2SDK "${_CLEAN_PS2DEV}/ps2sdk")
    set(_CLEAN_PATH "${_CLEAN_PS2DEV}/ee/bin:${_CLEAN_PS2DEV}/iop/bin:${_CLEAN_PS2DEV}/dvp/bin:$ENV{PATH}")

    # NOTE: CMake reserves the name "clean" for its own built-in target, so this
    # extended target is named "clean-all".  It cleans every artefact that the
    # built-in "clean" misses: thirdparty source objects and exec/ outputs.
    #
    # Usage:  cmake --build build --target clean-all
    add_custom_target(clean-all
        COMMENT "Cleaning all build artefacts for a full from-scratch rebuild..."
        # 1. CMake-tracked objects and libraries (engine, lua, .o files in build/).
        COMMAND ${CMAKE_MAKE_PROGRAM} clean
        # 2. Thirdparty cached libraries — deleting them forces CMake's
        #    add_custom_command to re-trigger the full thirdparty build next time.
        COMMAND ${CMAKE_COMMAND} -E rm -f
            "${CMAKE_SOURCE_DIR}/thirdparty/raylib/src/libraylib.a"
            "${CMAKE_SOURCE_DIR}/thirdparty/ps2gl/libps2gl.a"
        # 3. Raylib source objects (the raylib Makefile tracks these itself).
        COMMAND ${CMAKE_COMMAND} -E env
            "PS2DEV=${_CLEAN_PS2DEV}" "PS2SDK=${_CLEAN_PS2SDK}" "PATH=${_CLEAN_PATH}"
            ${CMAKE_MAKE_PROGRAM} -C "${CMAKE_SOURCE_DIR}/thirdparty/raylib/src" clean
        # 4. ps2gl source objects.
        COMMAND ${CMAKE_COMMAND} -E env
            "PS2DEV=${_CLEAN_PS2DEV}" "PS2SDK=${_CLEAN_PS2SDK}" "PATH=${_CLEAN_PATH}"
            ${CMAKE_MAKE_PROGRAM} -C "${CMAKE_SOURCE_DIR}/thirdparty/ps2gl" clean
        # 5. Final outputs in exec/.
        COMMAND ${CMAKE_COMMAND} -E rm -f
            "${CMAKE_SOURCE_DIR}/exec/${APP_EXE_NAME}"
            "${CMAKE_SOURCE_DIR}/exec/${APP_ISO_NAME}"
            "${CMAKE_SOURCE_DIR}/exec/main.sym"
        COMMAND ${CMAKE_COMMAND} -E rm -rf
            "${CMAKE_SOURCE_DIR}/exec/iso_contents"
        VERBATIM
    )
endif()

if(DEFINED ENV{PS2DEV})
    if(DEFINED ENV{PS2SDK})
        set(_CLANGD_SDK_PATH "$ENV{PS2SDK}")
    else()
        set(_CLANGD_SDK_PATH "$ENV{PS2DEV}/ps2sdk")
    endif()

    find_program(WSLPATH_BIN wslpath)
    if(WSLPATH_BIN)
        execute_process(
            COMMAND ${WSLPATH_BIN} -w "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE _CLANGD_ENG_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
            COMMAND ${WSLPATH_BIN} -w "${_CLANGD_SDK_PATH}"
            OUTPUT_VARIABLE _CLANGD_SDK_PATH_WIN
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        string(REPLACE "\\" "/" _CLANGD_ENG_PATH "${_CLANGD_ENG_PATH}")
        string(REPLACE "\\" "/" _CLANGD_SDK_PATH "${_CLANGD_SDK_PATH_WIN}")
    else()
        set(_CLANGD_ENG_PATH "${CMAKE_SOURCE_DIR}")
    endif()

    file(WRITE "${CMAKE_SOURCE_DIR}/.clangd"
        "CompileFlags:\n"
        "  Add:\n"
        "    - \"-I${_CLANGD_ENG_PATH}/engine/include\"\n"
        "    - \"-I${_CLANGD_ENG_PATH}/thirdparty/raylib/src\"\n"
        "    - \"-I${_CLANGD_ENG_PATH}/thirdparty/ps2gl/include\"\n"
        "    - \"-I${_CLANGD_ENG_PATH}/thirdparty/lua\"\n"
        "    - \"-I${_CLANGD_SDK_PATH}/ee/include\"\n"
        "    - \"-I${_CLANGD_SDK_PATH}/common/include\"\n"
        "    - \"-I${_CLANGD_SDK_PATH}/ports/include\"\n"
        "    - \"-D_PS2\"\n"
        "    - \"-D_EE\"\n"
        "    - \"-DPLATFORM_PLAYSTATION2\"\n"
        "\n"
        "  CompilationDatabase: build\n"
    )
    message(STATUS "Generated .clangd for IDE support")
endif()
