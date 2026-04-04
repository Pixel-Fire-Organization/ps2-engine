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


find_program(PYTHON3_BIN python3)
if(PYTHON3_BIN)
    add_custom_target(pack-assets
        COMMAND ${PYTHON3_BIN} "${CMAKE_SOURCE_DIR}/scripts/pack_assets.py"
            --src "${CMAKE_CURRENT_SOURCE_DIR}/cd_files/RAYLIB"
            --dst "${CMAKE_CURRENT_SOURCE_DIR}/cd_files/rassets"
        COMMENT "Packing assets from cd_files/RAYLIB -> cd_files/rassets..."
        VERBATIM
    )
    add_dependencies(${APP_EXE_NAME} pack-assets)
else()
    message(WARNING "python3 not found. Asset packing (pack_assets.py) will be skipped.")
endif()

if(DEBUG AND CMAKE_SYSTEM_NAME STREQUAL "Generic")
    get_filename_component(_TOOLCHAIN_BIN_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
    find_program(EE_NM_BIN
        NAMES mips64r5900el-ps2-elf-nm ee-nm
        PATHS "${_TOOLCHAIN_BIN_DIR}"
        NO_DEFAULT_PATH
    )
    if(EE_NM_BIN)
        get_filename_component(APP_EXE_BASENAME "${APP_EXE_NAME}" NAME_WE)
        add_custom_command(TARGET ${APP_EXE_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -D "EE_NM=${EE_NM_BIN}"
                -D "ELF=$<TARGET_FILE:${APP_EXE_NAME}>"
                -D "SYM=${CMAKE_SOURCE_DIR}/exec/${APP_EXE_BASENAME}.sym"
                -P "${CMAKE_SOURCE_DIR}/scripts/gen_sym.cmake"
            COMMENT "Generating PCSX2 debug symbols: exec/${APP_EXE_BASENAME}.sym"
            VERBATIM
        )
        message(STATUS "Debug .sym generation enabled (ee-nm: ${EE_NM_BIN})")
    else()
        message(WARNING "ee-nm not found — PCSX2 .sym generation skipped.")
    endif()
endif()

set(ISO_STAGING_DIR "${CMAKE_BINARY_DIR}/iso_root")
find_program(MKISOFS_BIN mkisofs)
if(MKISOFS_BIN)
    add_custom_command(TARGET ${APP_EXE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/exec"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ISO_STAGING_DIR}"

        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/cd_files" "${ISO_STAGING_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${APP_EXE_NAME}>" "${ISO_STAGING_DIR}/${APP_EXE_NAME_UPPER}"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/SYSTEM.CNF" "${ISO_STAGING_DIR}/SYSTEM.CNF"

        COMMAND ${MKISOFS_BIN} -l -o "${CMAKE_SOURCE_DIR}/exec/${APP_ISO_NAME}" "${ISO_STAGING_DIR}"
        COMMENT "Generating bootable PS2 iso: exec/${APP_ISO_NAME} with assets from app/cd_files"
    )
else()
    message(WARNING "mkisofs not found in environment. Skipping ISO bundle generation.")
endif()

if(DEBUG AND CMAKE_SYSTEM_NAME STREQUAL "Generic" AND MKISOFS_BIN)
    add_custom_command(TARGET ${APP_EXE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${ISO_STAGING_DIR}" "${CMAKE_SOURCE_DIR}/exec/iso_contents"
        COMMENT "Copying ISO contents to exec/iso_contents/..."
        VERBATIM
    )
endif()

