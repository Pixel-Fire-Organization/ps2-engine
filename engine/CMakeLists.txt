option(ENGINE_SANDBOX_MODE
    "Restrict the app target to the curated app_public headers only. \
OFF exposes the full engine include directory (dev/test mode)."
    ON
)

add_library(ps2_engine STATIC
        src/EngineCore.cpp
        src/EngineDebug.cpp
        src/EngineMemory.cpp
        src/EngineIO.cpp
        src/EngineArchive.cpp
        src/EngineResource.cpp
        src/EngineLevel.cpp
        src/EngineApp.cpp
        src/GameAPI.cpp
        src/EngineInput.cpp
        src/graphics/GLRenderer.cpp
        src/graphics/TagRenderer.cpp
        src/graphics/tim2.cpp
        src/graphics/ModelLoad.cpp
        src/graphics/Transform3D.cpp
        src/graphics/DrawList.cpp
        src/graphics/Frustum.cpp

        # Headers
        include/Engine.h
        include/EngineApp.h
        include/GameAPI.h
        include/app_public/GameAPI.h
        include/Constants.h
        include/Macros.h
        include/EngineCore.h
        include/EngineDebug.h
        include/EngineIO.h
        include/EngineArchive.h
        include/EngineMemory.h
        include/EngineResource.h
        include/EngineLevel.h
        include/EngineLevelFormat.h
        include/EngineInput.h
        include/graphics/EngineGraphics.h
        include/graphics/Renderer.h
        include/graphics/Primitives.h
        include/graphics/Types.h
        include/graphics/UI.h
        include/graphics/DrawList.h
        include/graphics/Frustum.h
        include/graphics/GLRenderer.h
        include/graphics/TagRenderer.h
        include/graphics/tim2.h
        include/graphics/ModelFormat.h
)

# Internal engine code always sees the full include directory.
target_include_directories(ps2_engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/external/ps2gl/include"
)

if(ENGINE_SANDBOX_MODE)
    # App can only see the curated app_public headers — memory, IO, resource,
    # and other engine internals are not reachable from app code.
    target_include_directories(ps2_engine PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include/app_public"
    )
    message(STATUS "ENGINE_SANDBOX_MODE: ON — app restricted to app_public headers")
else()
    # Dev/test mode: full engine header surface exposed.
    target_include_directories(ps2_engine PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/external/ps2gl/include"
    )
    message(STATUS "ENGINE_SANDBOX_MODE: OFF — full engine headers exposed")
endif()

target_compile_options(ps2_engine PRIVATE -Wall -Wextra -Werror)


