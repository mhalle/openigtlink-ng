# Portable replacement for `bash -c "cmp <(A case) <(B case)"`.
#
# Invoked from ctest via
#   ${CMAKE_COMMAND} -DSHIM=... -DUPSTREAM=... -DCASE=... \
#                    -DWORKDIR=... -P run_parity_compare.cmake
#
# Runs each emitter with $CASE as argv[1] and redirects stdout to
# a per-case temp file, then compares byte-for-byte via CMake's
# own compare_files. Works identically on Linux, macOS, and
# Windows — no bash, no process substitution.

if(NOT SHIM OR NOT UPSTREAM OR NOT CASE OR NOT WORKDIR)
    message(FATAL_ERROR
        "run_parity_compare.cmake: need -DSHIM -DUPSTREAM -DCASE -DWORKDIR")
endif()

set(SHIM_OUT     "${WORKDIR}/${CASE}.shim.bin")
set(UPSTREAM_OUT "${WORKDIR}/${CASE}.upstream.bin")

file(MAKE_DIRECTORY "${WORKDIR}")

execute_process(
    COMMAND "${SHIM}" "${CASE}"
    OUTPUT_FILE "${SHIM_OUT}"
    RESULT_VARIABLE rc_shim)
if(NOT rc_shim EQUAL 0)
    message(FATAL_ERROR
        "shim emitter failed for case=${CASE} (rc=${rc_shim})")
endif()

execute_process(
    COMMAND "${UPSTREAM}" "${CASE}"
    OUTPUT_FILE "${UPSTREAM_OUT}"
    RESULT_VARIABLE rc_ups)
if(NOT rc_ups EQUAL 0)
    message(FATAL_ERROR
        "upstream emitter failed for case=${CASE} (rc=${rc_ups})")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${SHIM_OUT}" "${UPSTREAM_OUT}"
    RESULT_VARIABLE rc_cmp)
if(NOT rc_cmp EQUAL 0)
    message(FATAL_ERROR
        "parity mismatch for case=${CASE}:\n"
        "  shim     = ${SHIM_OUT}\n"
        "  upstream = ${UPSTREAM_OUT}")
endif()
