# gen_sym.cmake — Generates a PCSX2-compatible .sym file from an ELF.
#
# Called at build time via:
#   cmake -D EE_NM=<path> -D ELF=<path> -D SYM=<path> -P gen_sym.cmake
#
# Output format (one entry per line):
#   <8-hex-digit-address> <symbol_name>

if(NOT EE_NM OR NOT ELF OR NOT SYM)
    message(FATAL_ERROR "gen_sym.cmake: EE_NM, ELF, and SYM must be set via -D")
endif()

execute_process(
    COMMAND "${EE_NM}" --numeric-sort "${ELF}"
    OUTPUT_VARIABLE NM_OUTPUT
    ERROR_QUIET
    RESULT_VARIABLE NM_RESULT
)

if(NOT NM_RESULT EQUAL 0)
    message(WARNING "gen_sym.cmake: ee-nm exited with error for ${ELF}")
    return()
endif()

set(SYM_CONTENT "")
string(REPLACE "\n" ";" NM_LINES "${NM_OUTPUT}")
foreach(LINE IN LISTS NM_LINES)
    # Keep only code and data symbols (T/t/B/b/D/d/R/r/W/w)
    if(LINE MATCHES "^([0-9a-f]+) [TtBbDdRrWw] (.+)$")
        string(APPEND SYM_CONTENT "${CMAKE_MATCH_1} ${CMAKE_MATCH_2}\n")
    endif()
endforeach()

file(WRITE "${SYM}" "${SYM_CONTENT}")

