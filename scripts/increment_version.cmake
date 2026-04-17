# increment_version.cmake — Reads the current build version directly from a
# committed C header, increments it by 1, and writes the header back in place.
# The header is both the persistent counter (committed to git) and the C include
# consumed by the compiler — no separate plain-text file is needed.
#
# Called at build time via add_custom_target in CMakeLists.txt:
#   cmake -D HEADER_FILE=<path> -D DEFINE_NAME=<name> -D GUARD_NAME=<name>
#         -P increment_version.cmake
#
# Required -D arguments:
#   HEADER_FILE  – path to the committed header that holds the current counter
#                  (e.g. engine/include/build_engine_version.h).  The file is
#                  read, the integer inside is incremented, and the file is
#                  written back with the new value.
#   DEFINE_NAME  – the #define macro name in the header  (e.g. ENGINE_BUILD_VERSION)
#   GUARD_NAME   – the include-guard macro name           (e.g. BUILD_ENGINE_VERSION_H)

if(NOT HEADER_FILE OR NOT DEFINE_NAME OR NOT GUARD_NAME)
    message(FATAL_ERROR "increment_version.cmake: HEADER_FILE, DEFINE_NAME, and GUARD_NAME must be set via -D")
endif()

file(READ "${HEADER_FILE}" _header_content)
string(REGEX MATCH "#define[ \t]+${DEFINE_NAME}[ \t]+[(][(]uint32_t[)]([0-9]+)u[)]" _match "${_header_content}")
if(_match STREQUAL "")
    message(FATAL_ERROR "increment_version.cmake: could not extract version number from ${HEADER_FILE}")
endif()

# Save the captured group before any further if(MATCHES) calls, which would
# overwrite CMAKE_MATCH_1 with the results of the new regex.
set(_current_version "${CMAKE_MATCH_1}")

if(NOT _current_version MATCHES "^[0-9]+$")
    message(FATAL_ERROR "increment_version.cmake: extracted version '${_current_version}' is not a valid non-negative integer")
endif()

math(EXPR _new_version "${_current_version} + 1")

file(WRITE "${HEADER_FILE}"
    "#ifndef ${GUARD_NAME}\n"
    "#define ${GUARD_NAME}\n"
    "\n"
    "#include <stdint.h>\n"
    "\n"
    "#define ${DEFINE_NAME} ((uint32_t)${_new_version}u)\n"
    "\n"
    "#endif /* ${GUARD_NAME} */\n"
)

message(STATUS "${DEFINE_NAME}: ${_new_version}")
