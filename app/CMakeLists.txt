add_executable(${APP_EXE_NAME} EXCLUDE_FROM_ALL src/main.c)
set_target_properties(${APP_EXE_NAME} PROPERTIES
    LINKER_LANGUAGE CXX
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/exec"
)
target_compile_options(${APP_EXE_NAME} PRIVATE -Wall -Wextra -Werror)

if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    target_link_directories(${APP_EXE_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/raylib/src
        ${CMAKE_SOURCE_DIR}/thirdparty/ps2gl
    )
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
endif()

# Generate a bootable ISO after the ELF is linked
find_program(MKISOFS_BIN mkisofs)
if(MKISOFS_BIN)
    set(ISO_STAGING_DIR "${CMAKE_BINARY_DIR}/iso_root")
    add_custom_command(TARGET ${APP_EXE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/exec"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ISO_STAGING_DIR}"
        # Copy CD files content to staging (if directory exists)
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/cd_files" "${ISO_STAGING_DIR}"
        # Copy ELF and SYSTEM.CNF to staging
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${APP_EXE_NAME}>" "${ISO_STAGING_DIR}/${APP_EXE_NAME_UPPER}"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/SYSTEM.CNF" "${ISO_STAGING_DIR}/SYSTEM.CNF"
        # Generate ISO from staging directory
        COMMAND ${MKISOFS_BIN} -l -o "${CMAKE_SOURCE_DIR}/exec/${APP_ISO_NAME}" "${ISO_STAGING_DIR}"
        COMMENT "Generating bootable PS2 iso: exec/${APP_ISO_NAME} with assets from app/cd_files"
    )
else()
    message(WARNING "mkisofs not found in environment. Skipping ISO bundle generation.")
endif()
