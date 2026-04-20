option(ENGINE_SANDBOX_MODE
    "Restrict the app target to EngineApp.h only (Lua-driven sandbox mode). \
OFF exposes the full engine include directory (dev/test mode)."
    ON
)

add_library(ps2_engine STATIC
        src/EngineCore.cpp
        src/EngineDebug.cpp
        src/EngineMemory.cpp
        src/EngineIO.cpp
        src/EngineResource.cpp
        src/EngineLevel.cpp
        src/EngineScript.cpp
        src/EngineApp.cpp
        src/EngineInput.cpp
        src/graphics/RaylibRenderer.cpp
        src/graphics/Transform3D.cpp
        src/graphics/DrawList.cpp

        # Headers
        include/Engine.h
        include/EngineApp.h
        include/Constants.h
        include/Macros.h
        include/EngineCore.h
        include/EngineDebug.h
        include/EngineIO.h
        include/EngineMemory.h
        include/EngineResource.h
        include/EngineLevel.h
        include/EngineScript.h
        include/EngineInput.h
        include/graphics/EngineGraphics.h
        include/graphics/Renderer.h
        include/graphics/Primitives.h
        include/graphics/UI.h
        include/graphics/RaylibRenderer.h
        include/graphics/DrawList.h
)

# Internal engine code always sees the full include directory.
target_include_directories(ps2_engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/thirdparty/raylib/src"
    "${CMAKE_SOURCE_DIR}/thirdparty/ps2gl/include"
    "${CMAKE_SOURCE_DIR}/thirdparty/lua"
)

if(ENGINE_SANDBOX_MODE)
    # App can only see EngineApp.h — memory, IO, resource, and scripting
    # internals are not reachable from app code.
    target_include_directories(ps2_engine PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include/app_public"
    )
    message(STATUS "ENGINE_SANDBOX_MODE: ON — app restricted to EngineApp.h")
else()
    # Dev/test mode: full engine header surface exposed.
    target_include_directories(ps2_engine PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/thirdparty/raylib/src"
        "${CMAKE_SOURCE_DIR}/thirdparty/ps2gl/include"
        "${CMAKE_SOURCE_DIR}/thirdparty/lua"
    )
    message(STATUS "ENGINE_SANDBOX_MODE: OFF — full engine headers exposed")
endif()

target_compile_options(ps2_engine PRIVATE -Wall -Wextra -Werror)


