# Portable replacement for `bash -c "cd DIR && ar qcs OUT *.o"`.
#
# Invoked as
#   ${CMAKE_COMMAND} -DAR=... -DMERGE_DIR=... -DOUT=... \
#                    -P ar_merge_objects.cmake
#
# Uses file(GLOB ...) to find all object files in MERGE_DIR (we've
# already extracted them there with `ar -x` in the caller) and
# passes them to `${AR} qcs ${OUT} obj1.o obj2.o ...`. Stays on
# POSIX branch only — Windows uses lib.exe with direct inputs and
# never needs this script.

if(NOT AR OR NOT MERGE_DIR OR NOT OUT)
    message(FATAL_ERROR
        "ar_merge_objects.cmake: need -DAR -DMERGE_DIR -DOUT")
endif()

file(GLOB OBJS RELATIVE "${MERGE_DIR}" "${MERGE_DIR}/*.o")
if(NOT OBJS)
    message(FATAL_ERROR
        "ar_merge_objects.cmake: no .o files in ${MERGE_DIR}")
endif()

execute_process(
    COMMAND "${AR}" qcs "${OUT}" ${OBJS}
    WORKING_DIRECTORY "${MERGE_DIR}"
    RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR
        "ar qcs failed (rc=${rc}) in ${MERGE_DIR}")
endif()
