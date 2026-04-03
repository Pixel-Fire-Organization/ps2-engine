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

# Define the target executable name
set(APP_EXE_NAME "main.elf")
set(APP_ISO_NAME "engine.iso")
set(APP_REGION ${REGION})

add_executable(${APP_EXE_NAME} test_app/Main.c)
set_target_properties(${APP_EXE_NAME} PROPERTIES 
    LINKER_LANGUAGE CXX
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/exec"
)

if(DEFINED ENV{PS2SDK})
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
