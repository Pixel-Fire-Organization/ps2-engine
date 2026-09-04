# PlayStation Vita toolchain. Builds both variants of the family.

if("$ENV{VITASDK}" STREQUAL "")
    message(FATAL_ERROR "VITASDK environment variable is not set!")
endif()

include("$ENV{VITASDK}/share/vita.toolchain.cmake")

set(ENGINE_TOOLCHAIN_ID "vitasdk" CACHE INTERNAL "Active toolchain")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Appended, not forced: the SDK adds -Wl,-q and vita-elf-create needs it.
if(NOT ENGINE_VITA_FLAGS_APPLIED)
    set(_VITA_COMMON_FLAGS "-O2 -Wall")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${_VITA_COMMON_FLAGS} -std=c99")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_VITA_COMMON_FLAGS} -std=c++11 -fno-exceptions -fno-rtti")
    set(ENGINE_VITA_FLAGS_APPLIED TRUE CACHE INTERNAL "Vita flag append guard")
endif()
