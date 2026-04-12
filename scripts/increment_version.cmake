# increment_version.cmake — Increments a build version counter and regenerates
# a C header with the new uint32_t value.
#
# Called at build time via add_custom_target in CMakeLists.txt:
#   cmake -D VERSION_FILE=<path> -D OUTPUT_HEADER=<path>
#         [-D DEFINE_NAME=<name>] [-D GUARD_NAME=<name>]
#         -P increment_version.cmake
#
# Required -D arguments:
#   VERSION_FILE   – path to the plain-text file holding the current counter
#                    (e.g. last_app_version.txt).  The integer it contains is
#                    incremented by 1 and written back.
#   OUTPUT_HEADER  – path where the generated C header is written.
#
# Optional -D arguments (sensible defaults shown):
#   DEFINE_NAME    – the #define macro name in the header  [BUILD_VERSION]
#   GUARD_NAME     – the include-guard macro name          [BUILD_VERSION_H]

if(NOT VERSION_FILE OR NOT OUTPUT_HEADER)
    message(FATAL_ERROR "increment_version.cmake: VERSION_FILE and OUTPUT_HEADER must be set via -D")
endif()

if(NOT DEFINE_NAME)
    set(DEFINE_NAME "BUILD_VERSION")
endif()

if(NOT GUARD_NAME)
    set(GUARD_NAME "BUILD_VERSION_H")
endif()

file(READ "${VERSION_FILE}" _version_str)
string(STRIP "${_version_str}" _version_str)
math(EXPR _new_version "${_version_str} + 1")

file(WRITE "${VERSION_FILE}" "${_new_version}")
file(WRITE "${OUTPUT_HEADER}"
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
