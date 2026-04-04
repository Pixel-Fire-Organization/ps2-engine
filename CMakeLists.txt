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

# Set definitions globally to ensure all targets (including tests/apps) see them
if(REGION STREQUAL "PAL")
    add_definitions(-DREGION_PAL=1)
else()
    add_definitions(-DREGION_NTSC=1)
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic")
    message(WARNING "You should build this project using the PS2 toolchain file. E.g.: cmake -DCMAKE_TOOLCHAIN_FILE=ps2dev.cmake -B build")
endif()

# Define the target executable name — must be set before subdirectories so children can reference it
set(APP_EXE_NAME "main.elf")
set(APP_ISO_NAME "engine.iso")
set(APP_REGION ${REGION})

# Compute uppercase name and generate SYSTEM.CNF before subdirs so app/ can reference APP_EXE_NAME_UPPER
string(TOUPPER ${APP_EXE_NAME} APP_EXE_NAME_UPPER)
file(WRITE "${CMAKE_BINARY_DIR}/SYSTEM.CNF" "BOOT2 = cdrom0:\\${APP_EXE_NAME_UPPER};1\r\nVER = 1.00\r\nVMODE = ${APP_REGION}\r\n")

add_subdirectory(thirdparty)
add_subdirectory(engine)
add_subdirectory(app)

if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    # Cross-platform emulator launcher — works on Linux natively and on Windows via WSL interop
    add_custom_target(run-emulator
        COMMAND bash "${CMAKE_SOURCE_DIR}/scripts/runEmulator.sh" "${CMAKE_SOURCE_DIR}/exec/engine.iso"
        COMMENT "Launching PCSX2..."
        VERBATIM
    )
endif()

# Generate .clangd at configure time for IDE/clangd support
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
