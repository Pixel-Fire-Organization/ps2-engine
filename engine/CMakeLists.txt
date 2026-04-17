option(ENGINE_SANDBOX_MODE
    "Restrict the app target to EngineApp.h only (Lua-driven sandbox mode). \
OFF exposes the full engine include directory (dev/test mode)."
    ON
)

# ---------------------------------------------------------------------------
# Engine build version — increments the counter in build_engine_version.h
# on every build. The header is both the persistent counter (committed to
# git) and the C include consumed by the compiler.
#
# The custom target has no OUTPUT so Make always runs it unconditionally.
# OBJECT_DEPENDS forces EngineCore.c to recompile after regeneration.
# ---------------------------------------------------------------------------
set(_ENGINE_VERSION_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/build_engine_version.h")

add_custom_target(increment_engine_version
    COMMAND ${CMAKE_COMMAND}
        -D "HEADER_FILE=${_ENGINE_VERSION_HEADER}"
        -D "DEFINE_NAME=ENGINE_BUILD_VERSION"
        -D "GUARD_NAME=BUILD_ENGINE_VERSION_H"
        -P "${CMAKE_SOURCE_DIR}/scripts/increment_version.cmake"
    COMMENT "Incrementing engine build version..."
    VERBATIM
)

add_library(ps2_engine STATIC
    src/EngineCore.c
    src/EngineDebug.c
    src/EngineMemory.c
    src/EngineIO.c
    src/EngineResource.c
    src/EngineLevel.c
    src/EngineScript.c
    src/EngineApp.c

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

add_dependencies(ps2_engine increment_engine_version)
set_source_files_properties(src/EngineCore.c PROPERTIES OBJECT_DEPENDS "${_ENGINE_VERSION_HEADER}")
