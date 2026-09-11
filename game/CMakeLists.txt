# ---------------------------------------------------------------------------
# The game.
#
# The sources and the asset pipeline are platform-neutral and defined once; the
# executable is created per active platform by game_add_executable(), each
# linking that platform engine library and landing in its own dist/ bundle.
# ---------------------------------------------------------------------------

set(GAME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Game.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EcsHooks.cpp"
    CACHE INTERNAL "")

find_program(PYTHON3_BIN python3 REQUIRED)

# --- ECS code generation (shared by every platform) -------------------------
# tools/ECS/ECS.json is the single source of truth for the component/entity C++
# and the TrenchBroom FGD. Generating at build time makes drift impossible; the
# FGD is copied back into the committed editor profile so TrenchBroom stays
# current (copy_if_different = no-op on a clean tree).
set(ECS_GEN_DIR "${CMAKE_BINARY_DIR}/generated/ecs" CACHE INTERNAL "")
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

# The app targets are created from the root scope, where a custom command OUTPUT
# in this directory is not visible as a dependency. A target bridges the two.
add_custom_target(ecs-generate
    DEPENDS "${ECS_GEN_DIR}/EcsComponents.h" "${ECS_GEN_DIR}/EcsSpawn.cpp"
)

# --- Achievement code generation (shared by every platform) -----------------
# game/config/achievements.json is the single source of truth for the set. The
# enum is what game code names an achievement by; the table is what the engine
# reads to show one. Generating at build time is what keeps the declaration,
# the engine and every platform's packaging describing one set.
set(ACHV_GEN_DIR "${CMAKE_BINARY_DIR}/generated/achievements" CACHE INTERNAL "")
set(ACHV_JSON   "${CMAKE_CURRENT_SOURCE_DIR}/config/achievements.json")
set(ACHV_GEN_PY "${CMAKE_SOURCE_DIR}/tools/achievements.py")

add_custom_command(
    OUTPUT  "${ACHV_GEN_DIR}/AchievementIds.h"
            "${ACHV_GEN_DIR}/AchievementTable.cpp"
    COMMAND ${PYTHON3_BIN} "${ACHV_GEN_PY}"
            --declaration "${ACHV_JSON}"
            --emit-ids   "${ACHV_GEN_DIR}/AchievementIds.h"
            --emit-table "${ACHV_GEN_DIR}/AchievementTable.cpp"
    DEPENDS "${ACHV_JSON}" "${ACHV_GEN_PY}"
    COMMENT "Generating achievements (AchievementIds.h, AchievementTable.cpp) from achievements.json"
    VERBATIM
)
set_source_files_properties(
    "${ACHV_GEN_DIR}/AchievementIds.h"
    "${ACHV_GEN_DIR}/AchievementTable.cpp"
    PROPERTIES GENERATED TRUE
)

add_custom_target(achievements-generate
    DEPENDS "${ACHV_GEN_DIR}/AchievementIds.h" "${ACHV_GEN_DIR}/AchievementTable.cpp"
)

# --- Stage 3: cook (per platform) -------------------------------------------
# Cooking is NOT shared: the right encoding is a hardware question, so each
# platform bakes from its own cook list into its own tree. See docs/PIPELINE.md.
#
# game_add_cook_target() is called once per surviving platform, below.
# Captured at this directory's scope: game_add_cook_target is called from the
# root list file, where CMAKE_CURRENT_SOURCE_DIR is not this directory.
set(GAME_ASSETS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cd_files/ASSETS" CACHE INTERNAL "Source assets")

function(game_add_cook_target PLATFORM)
    string(TOLOWER "${PLATFORM}" _lower)
    set(_cooked "${CMAKE_SOURCE_DIR}/dist/cooked/${_lower}")

    add_custom_target(cook-${_lower}
        COMMAND ${PYTHON3_BIN} "${CMAKE_SOURCE_DIR}/tools/cook_assets.py"
            --platform "${_lower}"
            --src "${GAME_ASSETS_SRC_DIR}"
            --src "${ENGINE_ASSETS_SRC_DIR}"
            --themes "${CMAKE_SOURCE_DIR}/game/config/theme.json"
            --dst "${_cooked}/rassets"
        COMMENT "Cook: engine + game source assets -> dist/cooked/${_lower}/rassets"
        VERBATIM
    )

    # Stage 4 is gated on validation, so a bad cook can never reach a container.
    add_custom_target(package-${_lower}
        COMMAND ${PYTHON3_BIN} "${CMAKE_SOURCE_DIR}/tools/validate_cooked.py"
            --platform "${_lower}"
        COMMENT "Package: validate dist/cooked/${_lower}, then build containers"
        VERBATIM
    )
    add_dependencies(package-${_lower} cook-${_lower} compile-levels)
endfunction()

set(LEVELS_OUT_DIR "${CMAKE_BINARY_DIR}/levels" CACHE INTERNAL "Compiled level archives")
file(GLOB LEVEL_MAPS "${CMAKE_SOURCE_DIR}/assets/maps/*.map")
add_custom_target(compile-levels
    COMMAND ${CMAKE_COMMAND} -E make_directory "${LEVELS_OUT_DIR}"
    COMMENT "Compiling levels from assets/maps/*.map -> ${LEVELS_OUT_DIR}"
)
foreach(MAP_FILE ${LEVEL_MAPS})
    add_custom_command(TARGET compile-levels POST_BUILD
        COMMAND ${PYTHON3_BIN} "${CMAKE_SOURCE_DIR}/tools/compile_level.py"
                "${MAP_FILE}" --out "${LEVELS_OUT_DIR}"
                --textures "${CMAKE_SOURCE_DIR}/assets/textures"
                --models "${CMAKE_SOURCE_DIR}/assets/models"
        VERBATIM
    )
endforeach()

# Levels compile once: their format is identical on every platform, so unlike
# textures there is nothing platform-specific to decide.
add_custom_target(stage-assets)
add_dependencies(stage-assets compile-levels)

# --- Per-platform executable ------------------------------------------------
function(game_add_executable PLATFORM ENGINE_TARGET OUT_TARGET OUT_DIST_DIR)
    string(TOLOWER "${PLATFORM}" _lower)
    set(_target "app_${_lower}")
    set(_dist "${CMAKE_SOURCE_DIR}/dist/${PLATFORM_${PLATFORM}_DIST}")

    # GENERATED is a directory-scoped property, so re-assert it here: this
    # function runs in the root scope, which has not seen the custom command.
    set_source_files_properties("${ECS_GEN_DIR}/EcsSpawn.cpp" PROPERTIES GENERATED TRUE)
    set_source_files_properties("${ACHV_GEN_DIR}/AchievementTable.cpp" PROPERTIES GENERATED TRUE)

    add_executable(${_target} EXCLUDE_FROM_ALL ${GAME_SOURCES}
        "${ECS_GEN_DIR}/EcsSpawn.cpp"
        "${ACHV_GEN_DIR}/AchievementTable.cpp")

    # SUFFIX "" because PLATFORM_<P>_EXE is the complete filename: CMake would
    # otherwise append its own platform suffix and produce game.exe.exe.
    set_target_properties(${_target} PROPERTIES
        LINKER_LANGUAGE CXX
        OUTPUT_NAME "${PLATFORM_${PLATFORM}_EXE}"
        SUFFIX ""
        RUNTIME_OUTPUT_DIRECTORY "${_dist}"
    )
    target_include_directories(${_target} PRIVATE "${ECS_GEN_DIR}" "${ACHV_GEN_DIR}")
    target_link_libraries(${_target} PRIVATE ${ENGINE_TARGET})
    target_compile_options(${_target} PRIVATE -Wall -Wextra -Werror)
    add_dependencies(${_target} ecs-generate achievements-generate stage-assets package-${_lower})

    set(${OUT_TARGET} "${_target}" PARENT_SCOPE)
    set(${OUT_DIST_DIR} "${_dist}" PARENT_SCOPE)
endfunction()
