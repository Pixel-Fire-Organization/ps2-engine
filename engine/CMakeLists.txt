add_library(ps2_engine STATIC
    src/EngineCore.c
    src/EngineDebug.c
    src/EngineMemory.c
    src/EngineIO.c
    src/EngineScript.c

    # Headers
    include/Engine.h
    include/EngineCore.h
    include/EngineDebug.h
    include/EngineIO.h
    include/EngineMemory.h
    include/EngineScript.h
)
target_include_directories(ps2_engine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/thirdparty/raylib/src"
    "${CMAKE_SOURCE_DIR}/thirdparty/ps2gl/include"
    "${CMAKE_SOURCE_DIR}/thirdparty/lua"
)
target_compile_options(ps2_engine PRIVATE -Wall -Wextra -Werror)
