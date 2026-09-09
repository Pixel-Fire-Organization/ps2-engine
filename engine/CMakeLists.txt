# ---------------------------------------------------------------------------
# The engine library, built once per active platform.
#
# Each platform gets its own library rather than one shared build: platform
# constants are compile-time (PlatformConstants.h resolves through the include
# path), and the PAL/NTSC screen dimensions in particular are multiplied per
# transformed vertex in the GIFTAG renderer, where a runtime lookup would cost
# the EE dearly.
# ---------------------------------------------------------------------------

set(ENGINE_CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CommandLine.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EngineMain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/PlatformRegistry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EngineCore.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EngineDebug.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EngineNotice.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EngineAchievementUi.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EngineMemory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EngineSubsystems.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/PlatformContract.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EngineIO.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EngineArchive.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EngineResource.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EngineLevel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EngineSector.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EngineApp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/GameAPI.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EngineInput.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EngineAchievement.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/Ui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/UiWidgets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/UiFont.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/UiStyle.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/NullRenderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/tim2.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/ModelLoad.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/Transform3D.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/DrawList.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/StagedGeometry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/TextureExpand.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/Frustum.cpp"
    CACHE INTERNAL "")

set(ENGINE_CORE_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/CommandLine.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EngineMain.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/platform/Platform.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/platform/MemoryContract.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/platform/PlatformKeys.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/platform/PlatformRegistry.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/platform/PlatformTypes.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Engine.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EngineApp.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/GameAPI.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Macros.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EngineCore.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EngineDebug.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EngineNotice.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EngineAchievementUi.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EngineIO.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EngineArchive.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EngineMemory.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EngineSubsystems.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EngineResource.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EngineLevel.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EngineLevelFormat.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EngineSector.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EngineInput.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EngineAchievement.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/platform/AchievementContract.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graphics/EngineGraphics.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graphics/NullRenderer.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graphics/Renderer.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graphics/Primitives.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graphics/Types.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graphics/UI.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/EngineUi.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graphics/DrawList.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graphics/StagedGeometry.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graphics/TextureExpand.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graphics/Frustum.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graphics/ModelFormat.h"
    CACHE INTERNAL "")

set(ENGINE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include" CACHE INTERNAL "")

# The debug testbed is present only in a debug configuration: not switched off
# at run time, absent. Which sources are compiled expresses that, the same way
# it expresses platform choice. See docs/subsystems/TESTBED.md.
if(DEBUG)
    set(ENGINE_DEBUG_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/Testbed.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/TestbedScenes.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/ScenePlatformInfo.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/SceneGamepad.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/SceneKeyboardMouse.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/SceneTouch.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/ScenePointer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/SceneChords.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/ScenePrimitives.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/SceneScreenAspect.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/SceneDepthRange.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/SceneUiGallery.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/SceneStyle.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/SceneDrawLoad.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/SceneMemory.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/SceneResources.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/SceneAssetBrowser.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/SceneLevelStream.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/SceneAchievements.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/ScenePerformance.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/scenes/SceneFramePacing.cpp"
        CACHE INTERNAL "")
    set(ENGINE_DEBUG_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/debug" CACHE INTERNAL "")
else()
    set(ENGINE_DEBUG_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/debug/Stub.cpp"
        CACHE INTERNAL "")
    set(ENGINE_DEBUG_INCLUDE_DIR "" CACHE INTERNAL "")
endif()


find_program(PYTHON3_BIN python3 REQUIRED)

# --- Title identity ---------------------------------------------------------
# game/title.json is the single source of who made the title and what each
# platform files it under. A platform builds its writable-storage location from
# this, so the identity a console shows and the identity a save is filed under
# cannot disagree.
set(TITLE_GEN_DIR "${CMAKE_BINARY_DIR}/generated/title" CACHE INTERNAL "")
set(TITLE_JSON   "${CMAKE_SOURCE_DIR}/game/title.json")
set(TITLE_GEN_PY "${CMAKE_SOURCE_DIR}/tools/title.py")

add_custom_command(
    OUTPUT  "${TITLE_GEN_DIR}/TitleInfo.h"
    COMMAND ${PYTHON3_BIN} "${TITLE_GEN_PY}"
            --declaration "${TITLE_JSON}"
            --emit-header "${TITLE_GEN_DIR}/TitleInfo.h"
    DEPENDS "${TITLE_JSON}" "${TITLE_GEN_PY}"
    COMMENT "Generating TitleInfo.h from title.json"
    VERBATIM
)
set_source_files_properties("${TITLE_GEN_DIR}/TitleInfo.h" PROPERTIES GENERATED TRUE)
add_custom_target(title-generate DEPENDS "${TITLE_GEN_DIR}/TitleInfo.h")


# Build the engine library for one platform. That platform.cmake fragment has
# already run and set ENGINE_PLATFORM_<P>_SOURCES / _LIBS / _DEFINES / _INCLUDES.
function(engine_add_library PLATFORM OUT_TARGET)
    string(TOLOWER "${PLATFORM}" _lower)
    set(_target "engine_${_lower}")

    engine_platform_dirs(${PLATFORM} _variantDir _baseDir)

    add_library(${_target} STATIC
        ${ENGINE_CORE_SOURCES}
        ${ENGINE_CORE_HEADERS}
        ${ENGINE_DEBUG_SOURCES}
        ${ENGINE_PLATFORM_${PLATFORM}_SOURCES}
    )

    # "PlatformConstants.h" resolves here: the variant directory first, then the
    # shared base. This is what lets platform constants stay compile-time without
    # any shared engine file naming a platform.
    target_include_directories(${_target} PUBLIC
        "${ENGINE_INCLUDE_DIR}"
        "${ENGINE_DEBUG_INCLUDE_DIR}"
        "${TITLE_GEN_DIR}"
        "${_variantDir}"
    )
    add_dependencies(${_target} title-generate)
    if(_baseDir)
        target_include_directories(${_target} PUBLIC "${_baseDir}")
    endif()
    if(ENGINE_PLATFORM_${PLATFORM}_INCLUDES)
        target_include_directories(${_target} PUBLIC ${ENGINE_PLATFORM_${PLATFORM}_INCLUDES})
    endif()

    if(ENGINE_PLATFORM_${PLATFORM}_DEFINES)
        target_compile_definitions(${_target} PUBLIC ${ENGINE_PLATFORM_${PLATFORM}_DEFINES})
    endif()
    if(ENGINE_PLATFORM_${PLATFORM}_LIBS)
        target_link_libraries(${_target} PUBLIC ${ENGINE_PLATFORM_${PLATFORM}_LIBS})
    endif()

    target_compile_options(${_target} PRIVATE -Wall -Wextra -Werror)
    set_target_properties(${_target} PROPERTIES LINKER_LANGUAGE CXX)

    set(${OUT_TARGET} "${_target}" PARENT_SCOPE)
endfunction()
