# Cross-port parity comparator for the core-c target.
#
# Runs a C emitter (`${SHIM} ${CASE}`) and a Python emitter
# (`${UPSTREAM} ${UPSTREAM_ARG0} ${CASE}`) and compares their
# stdout byte-for-byte with `cmake -E compare_files`.
#
# This is a sibling of core-cpp/cmake/run_parity_compare.cmake
# — a separate copy because the upstream side here is a Python
# script (interpreter + script-path + case), not just a binary.
# Factoring the two into one script would tangle the interfaces
# more than it'd save.

if(NOT SHIM OR NOT UPSTREAM OR NOT UPSTREAM_ARG0 OR NOT CASE OR NOT WORKDIR)
    message(FATAL_ERROR
        "run_parity_compare.cmake: need -DSHIM -DUPSTREAM "
        "-DUPSTREAM_ARG0 -DCASE -DWORKDIR")
endif()

file(MAKE_DIRECTORY "${WORKDIR}")

set(SHIM_OUT     "${WORKDIR}/${CASE}.c.bin")
set(UPSTREAM_OUT "${WORKDIR}/${CASE}.py.bin")

execute_process(
    COMMAND "${SHIM}" "${CASE}"
    OUTPUT_FILE "${SHIM_OUT}"
    RESULT_VARIABLE rc_shim)
if(NOT rc_shim EQUAL 0)
    message(FATAL_ERROR
        "C emitter failed for case=${CASE} (rc=${rc_shim})")
endif()

execute_process(
    COMMAND "${UPSTREAM}" "${UPSTREAM_ARG0}" "${CASE}"
    OUTPUT_FILE "${UPSTREAM_OUT}"
    RESULT_VARIABLE rc_ups)
if(NOT rc_ups EQUAL 0)
    message(FATAL_ERROR
        "Python emitter failed for case=${CASE} (rc=${rc_ups})")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${SHIM_OUT}" "${UPSTREAM_OUT}"
    RESULT_VARIABLE rc_cmp)
if(NOT rc_cmp EQUAL 0)
    message(FATAL_ERROR
        "parity mismatch for case=${CASE}:\n"
        "  c     = ${SHIM_OUT}\n"
        "  py    = ${UPSTREAM_OUT}")
endif()
