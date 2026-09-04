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
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graphics/DrawList.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graphics/StagedGeometry.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graphics/TextureExpand.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graphics/Frustum.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graphics/ModelFormat.h"
    CACHE INTERNAL "")

set(ENGINE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include" CACHE INTERNAL "")

# Build the engine library for one platform. That platform.cmake fragment has
# already run and set ENGINE_PLATFORM_<P>_SOURCES / _LIBS / _DEFINES / _INCLUDES.
function(engine_add_library PLATFORM OUT_TARGET)
    string(TOLOWER "${PLATFORM}" _lower)
    set(_target "engine_${_lower}")

    engine_platform_dirs(${PLATFORM} _variantDir _baseDir)

    add_library(${_target} STATIC
        ${ENGINE_CORE_SOURCES}
        ${ENGINE_CORE_HEADERS}
        ${ENGINE_PLATFORM_${PLATFORM}_SOURCES}
    )

    # "PlatformConstants.h" resolves here: the variant directory first, then the
    # shared base. This is what lets platform constants stay compile-time without
    # any shared engine file naming a platform.
    target_include_directories(${_target} PUBLIC
        "${ENGINE_INCLUDE_DIR}"
        "${_variantDir}"
    )
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
