cmake_minimum_required(VERSION 3.10)
project(ps2_engine C CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic")
    message(WARNING "You should build this project using the PS2 toolchain file. E.g.: cmake -DCMAKE_TOOLCHAIN_FILE=ps2dev.cmake -B build")
endif()

add_library(ps2_engine STATIC
    src/engine_core.c
    src/engine_debug.c
    src/engine_memory.c
    src/engine_io.c
    src/engine_script.c
)
add_subdirectory(thirdparty/lua)
target_include_directories(ps2_engine PUBLIC include)

# Define the target executable name
set(APP_EXE_NAME "main.elf")
set(APP_ISO_NAME "engine.iso")
set(APP_REGION "PAL")

add_executable(${APP_EXE_NAME} test_app/main.c)
set_target_properties(${APP_EXE_NAME} PROPERTIES 
    LINKER_LANGUAGE CXX
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/exec"
)

if(DEFINED ENV{PS2SDK})
    target_include_directories(ps2_engine PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/raylib/src ${CMAKE_SOURCE_DIR}/thirdparty/ps2gl/include)
    target_include_directories(${APP_EXE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/raylib/src ${CMAKE_SOURCE_DIR}/thirdparty/ps2gl/include)
    target_link_directories(${APP_EXE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/raylib/src ${CMAKE_SOURCE_DIR}/thirdparty/ps2gl)
    target_link_libraries(${APP_EXE_NAME} PRIVATE 
        ps2_engine 
        lua
        raylib ps2gl ps2stuff dma graph draw pad math3d gs png z m mc c -ldebug
    )
else()
    target_link_libraries(${APP_EXE_NAME} PRIVATE ps2_engine)
endif()

# Dynamically generate SYSTEM.CNF into the build system so it perfectly matches the uppercase executable name
string(TOUPPER ${APP_EXE_NAME} APP_EXE_NAME_UPPER)
file(WRITE "${CMAKE_BINARY_DIR}/SYSTEM.CNF" "BOOT2 = cdrom0:\\${APP_EXE_NAME_UPPER};1\nVER = 1.00\nVMODE = ${APP_REGION}\n")

# Add a custom target to automatically generate the ISO file upon successful ELF build
find_program(MKISOFS_BIN mkisofs)
if(MKISOFS_BIN)
    add_custom_command(TARGET ${APP_EXE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/exec"
        COMMAND ${MKISOFS_BIN} -l -o "${CMAKE_SOURCE_DIR}/exec/${APP_ISO_NAME}" $<TARGET_FILE:${APP_EXE_NAME}> "${CMAKE_BINARY_DIR}/SYSTEM.CNF"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating bootable PS2 iso: exec/${APP_ISO_NAME}"
    )
else()
    message(WARNING "mkisofs not found in environment. Skipping ISO bundle generation.")
endif()
