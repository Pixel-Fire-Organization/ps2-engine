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

add_library(ps2_engine STATIC
    src/EngineCore.c
    src/EngineDebug.c
    src/EngineMemory.c
    src/EngineIO.c
    src/EngineScript.c
    
    # Headers
    include/Engine.h
    include/EngineCore.h
    include/EngineDebug.h
    include/EngineIO.h
    include/EngineMemory.h
    include/EngineScript.h
)
target_include_directories(ps2_engine PUBLIC 
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/thirdparty/raylib/src"
    "${CMAKE_SOURCE_DIR}/thirdparty/ps2gl/include"
)

add_subdirectory(thirdparty)

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
        COMMAND ${CMAKE_COMMAND} -E env "PS2DEV=${_PS2DEV}" "PS2SDK=${_PS2SDK}" "PATH=${_PS2_PATH}" ${CMAKE_MAKE_PROGRAM}
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
        COMMAND ${CMAKE_COMMAND} -E env "PS2DEV=${_PS2DEV}" "PS2SDK=${_PS2SDK}" "PATH=${_PS2_PATH}" ${CMAKE_MAKE_PROGRAM} PLATFORM=PLATFORM_PLAYSTATION2 "GRAPHICS=GRAPHICS_API_OPENGL_11 -I../../ps2gl/include"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/thirdparty/raylib/src"
        DEPENDS "${CMAKE_SOURCE_DIR}/thirdparty/ps2gl/libps2gl.a"
        COMMENT "Building raylib for PS2..."
        VERBATIM
    )
    add_custom_target(raylib_build
        DEPENDS "${CMAKE_SOURCE_DIR}/thirdparty/raylib/src/libraylib.a"
    )
endif()

# Define the target executable name
set(APP_EXE_NAME "main.elf")
set(APP_ISO_NAME "engine.iso")
set(APP_REGION ${REGION})

add_executable(${APP_EXE_NAME} EXCLUDE_FROM_ALL test_app/Main.c)
set_target_properties(${APP_EXE_NAME} PROPERTIES 
    LINKER_LANGUAGE CXX
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/exec"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    target_link_directories(${APP_EXE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/raylib/src ${CMAKE_SOURCE_DIR}/thirdparty/ps2gl)
    target_link_libraries(${APP_EXE_NAME} PRIVATE 
        ps2_engine 
        lua
        ${CMAKE_SOURCE_DIR}/thirdparty/raylib/src/libraylib.a
        ${CMAKE_SOURCE_DIR}/thirdparty/ps2gl/libps2gl.a
        ps2stuff dma graph draw pad math3d gs png z m mc c -ldebug
    )
else()
    target_link_libraries(${APP_EXE_NAME} PRIVATE ps2_engine)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    add_custom_command(TARGET ${APP_EXE_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_SOURCE_DIR}/exec"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/exec"
        COMMENT "Cleaning exec/..."
        VERBATIM
    )
    add_dependencies(${APP_EXE_NAME} ps2gl_build raylib_build)

    # Cross-platform emulator launcher — works on Linux natively and on Windows via WSL interop
    add_custom_target(run-emulator
        COMMAND bash "${CMAKE_SOURCE_DIR}/scripts/runEmulator.sh" "${CMAKE_SOURCE_DIR}/exec/engine.iso"
        COMMENT "Launching PCSX2..."
        VERBATIM
    )
endif()

# Dynamically generate SYSTEM.CNF into the build system so it perfectly matches the uppercase executable name
string(TOUPPER ${APP_EXE_NAME} APP_EXE_NAME_UPPER)
file(WRITE "${CMAKE_BINARY_DIR}/SYSTEM.CNF" "BOOT2 = cdrom0:\\${APP_EXE_NAME_UPPER};1\r\nVER = 1.00\r\nVMODE = ${APP_REGION}\r\n")

# Add a custom target to automatically generate the ISO file upon successful ELF build
find_program(MKISOFS_BIN mkisofs)
if(MKISOFS_BIN)
    set(ISO_STAGING_DIR "${CMAKE_BINARY_DIR}/iso_root")
    add_custom_command(TARGET ${APP_EXE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/exec"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ISO_STAGING_DIR}"
        # Copy CD files content to staging (if directory exists)
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/test_app/cd_files" "${ISO_STAGING_DIR}"
        # Copy ELF and SYSTEM.CNF to staging
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${APP_EXE_NAME}>" "${ISO_STAGING_DIR}/${APP_EXE_NAME_UPPER}"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/SYSTEM.CNF" "${ISO_STAGING_DIR}/SYSTEM.CNF"
        # Generate ISO from staging directory
        COMMAND ${MKISOFS_BIN} -l -o "${CMAKE_SOURCE_DIR}/exec/${APP_ISO_NAME}" "${ISO_STAGING_DIR}"
        COMMENT "Generating bootable PS2 iso: exec/${APP_ISO_NAME} with assets from test_app/cd_files"
    )
else()
    message(WARNING "mkisofs not found in environment. Skipping ISO bundle generation.")
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
        "    - \"-I${_CLANGD_ENG_PATH}/include\"\n"
        "    - \"-I${_CLANGD_ENG_PATH}/thirdparty/raylib/src\"\n"
        "    - \"-I${_CLANGD_ENG_PATH}/thirdparty/ps2gl/include\"\n"
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
