add_executable(${APP_EXE_NAME} EXCLUDE_FROM_ALL
    src/main.cpp
    src/Game.cpp
    src/SwarmSystem.cpp
)
set_target_properties(${APP_EXE_NAME} PROPERTIES
    LINKER_LANGUAGE CXX
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist"
)
target_compile_options(${APP_EXE_NAME} PRIVATE -Wall -Wextra -Werror)

if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    target_link_libraries(${APP_EXE_NAME} PRIVATE
        ps2_engine
        ${PS2GL_LIB}
            ps2stuff dma packet2 graph draw input pad math3d gs png z m mc c -ldebug
    )
else()
    target_link_libraries(${APP_EXE_NAME} PRIVATE ps2_engine)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    add_custom_command(TARGET ${APP_EXE_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f
            "${CMAKE_SOURCE_DIR}/dist/${APP_EXE_NAME}"
            "${CMAKE_SOURCE_DIR}/dist/${APP_ISO_NAME}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_SOURCE_DIR}/dist/iso_contents"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/dist"
        COMMENT "Cleaning dist/..."
        VERBATIM
    )
    add_dependencies(${APP_EXE_NAME} ps2gl_build)
endif()


find_program(PYTHON3_BIN python3)
if(PYTHON3_BIN)
    add_custom_target(pack-assets
        COMMAND ${PYTHON3_BIN} "${CMAKE_SOURCE_DIR}/tools/pack_assets.py"
            --src "${CMAKE_CURRENT_SOURCE_DIR}/cd_files/ASSETS"
            --dst "${CMAKE_CURRENT_SOURCE_DIR}/cd_files/rassets"
        COMMENT "Packing assets from cd_files/ASSETS -> cd_files/rassets..."
        VERBATIM
    )
    add_dependencies(${APP_EXE_NAME} pack-assets)
else()
    message(WARNING "python3 not found. Asset packing (pack_assets.py) will be skipped.")
endif()

# --- ECS code generation ----------------------------------------------------
# The game owns its ECS: tools/ECS/ECS.json is the single source of truth for the
# component/entity C++ (EcsComponents.h + EcsSpawn.cpp, generated into the build
# dir) AND the TrenchBroom FGD. Generating at build time makes drift impossible;
# the FGD is also copied back into the committed editor profile so TrenchBroom on
# the Windows host stays current (copy_if_different = no-op on a clean tree).
if(PYTHON3_BIN)
    set(ECS_GEN_DIR       "${CMAKE_BINARY_DIR}/generated/ecs")
    set(ECS_JSON          "${CMAKE_SOURCE_DIR}/tools/ECS/ECS.json")
    set(ECS_SCHEMA        "${CMAKE_SOURCE_DIR}/tools/ECS/schema.json")
    set(ECS_GEN_PY        "${CMAKE_SOURCE_DIR}/tools/ECS/generate_ecs.py")
    set(ECS_FGD_COMMITTED "${CMAKE_SOURCE_DIR}/tools/trenchbroom/games/PS2Engine/PS2Engine.fgd")

    add_custom_command(
        OUTPUT  "${ECS_GEN_DIR}/EcsComponents.h"
                "${ECS_GEN_DIR}/EcsSpawn.cpp"
                "${ECS_GEN_DIR}/PS2Engine.fgd"
        COMMAND ${PYTHON3_BIN} "${ECS_GEN_PY}"
                --ecs "${ECS_JSON}" --schema "${ECS_SCHEMA}"
                --out-header "${ECS_GEN_DIR}/EcsComponents.h"
                --out-source "${ECS_GEN_DIR}/EcsSpawn.cpp"
                --out-fgd    "${ECS_GEN_DIR}/PS2Engine.fgd"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ECS_GEN_DIR}/PS2Engine.fgd" "${ECS_FGD_COMMITTED}"
        DEPENDS "${ECS_JSON}" "${ECS_SCHEMA}" "${ECS_GEN_PY}"
        COMMENT "Generating ECS (EcsComponents.h, EcsSpawn.cpp, FGD) from ECS.json"
        VERBATIM
    )
    set_source_files_properties(
        "${ECS_GEN_DIR}/EcsComponents.h"
        "${ECS_GEN_DIR}/EcsSpawn.cpp"
        PROPERTIES GENERATED TRUE
    )

    target_sources(${APP_EXE_NAME} PRIVATE
        src/EcsHooks.cpp
        "${ECS_GEN_DIR}/EcsSpawn.cpp"
    )
    target_include_directories(${APP_EXE_NAME} PRIVATE "${ECS_GEN_DIR}")
else()
    message(FATAL_ERROR "python3 not found — required to generate ECS code (tools/ECS/generate_ecs.py).")
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
                -D "SYM=${CMAKE_SOURCE_DIR}/dist/${APP_EXE_BASENAME}.sym"
                -P "${CMAKE_SOURCE_DIR}/tools/gen_sym.cmake"
            COMMENT "Generating PCSX2 debug symbols: dist/${APP_EXE_BASENAME}.sym"
            VERBATIM
        )
        message(STATUS "Debug .sym generation enabled (ee-nm: ${EE_NM_BIN})")
    else()
        message(WARNING "ee-nm not found — PCSX2 .sym generation skipped.")
    endif()
endif()

set(ISO_STAGING_DIR "${CMAKE_BINARY_DIR}/iso_root")

set(ISO_CONTENT_BLACKLIST
    "ASSETS"   # Raw asset sources; packed output lives in rassets/
)

find_program(MKISOFS_BIN mkisofs)
if(MKISOFS_BIN)
    set(ISO_OUTPUT "${CMAKE_SOURCE_DIR}/dist/${APP_ISO_NAME}")

    # Always regenerate the ISO on every build, just like pack-assets.
    # Using add_custom_target (no OUTPUT) ensures the commands run unconditionally
    # so edited configs or packed assets are always included.
    add_custom_target(generate-iso
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/dist"
        # Wipe and recreate the staging dir so stale files from previous builds
        # (e.g. an old executable under a different name) never end up in the ISO.
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${ISO_STAGING_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ISO_STAGING_DIR}"
        COMMAND ${CMAKE_COMMAND}
                -D "SRC=${CMAKE_CURRENT_SOURCE_DIR}/cd_files"
                -D "DST=${ISO_STAGING_DIR}"
                -D "BLACKLIST=${ISO_CONTENT_BLACKLIST}"
                -P "${CMAKE_SOURCE_DIR}/tools/copy_iso_files.cmake"
        # Bundle the packed assets into a single flat archive at the disc root.
        # The engine mounts RASSETS.PS2R at boot and serves assets from it; the
        # loose rassets/ tree is still copied above as a fallback during the
        # loose->archive transition (dropped in a later phase).
        COMMAND ${PYTHON3_BIN} "${CMAKE_SOURCE_DIR}/tools/pack_archive.py"
                --dir "${CMAKE_CURRENT_SOURCE_DIR}/cd_files/rassets"
                --prefix RASSETS
                --dst "${ISO_STAGING_DIR}/RASSETS.PS2R"
        COMMAND ${CMAKE_COMMAND} -E copy
                "$<TARGET_FILE:${APP_EXE_NAME}>" "${ISO_STAGING_DIR}/${APP_SERIAL}"
        COMMAND ${CMAKE_COMMAND} -E copy
                "${CMAKE_BINARY_DIR}/SYSTEM.CNF" "${ISO_STAGING_DIR}/SYSTEM.CNF"
        # -sysid "PLAYSTATION" : sets the System Identifier field in the ISO 9660
        #   Primary Volume Descriptor.  OPL2 Manager (and other PS2 disc tools) check
        #   this field to confirm the image is a PS2 disc; the default ("LINUX") causes
        #   them to report the ISO as invalid.
        # -V "PS2ENGINE" : volume label shown by disc managers and used as the game
        #   identifier.  A non-empty label also prevents genisoimage from defaulting to
        #   the generic "CDROM" label.
        COMMAND ${MKISOFS_BIN} -l -sysid "PLAYSTATION" -V "${APP_SERIAL_LABEL}"
                -o "${ISO_OUTPUT}" "${ISO_STAGING_DIR}"
        COMMENT "Generating bootable PS2 iso: dist/${APP_ISO_NAME}"
        VERBATIM
    )

    if(DEBUG AND CMAKE_SYSTEM_NAME STREQUAL "Generic")
        add_custom_command(TARGET generate-iso POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${ISO_STAGING_DIR}" "${CMAKE_SOURCE_DIR}/dist/iso_contents"
            COMMENT "Copying ISO contents to dist/iso_contents/..."
            VERBATIM
        )
    endif()
else()
    # mkisofs not available — generate-iso still exists so `--target generate-iso`
    # in build.sh doesn't fail; it just builds the ELF without packaging.
    add_custom_target(generate-iso
        COMMENT "mkisofs not found — skipping ISO generation"
    )
    message(WARNING "mkisofs not found in environment. Skipping ISO bundle generation.")
endif()

# ELF and asset packing must finish before the ISO is assembled.
add_dependencies(generate-iso ${APP_EXE_NAME})
if(PYTHON3_BIN)
    add_dependencies(generate-iso pack-assets)
endif()

