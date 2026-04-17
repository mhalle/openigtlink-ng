# upstream_oracle_cli — 6th differential-fuzzer oracle.
#
# Wraps the pinned upstream OpenIGTLink C++ reference library
# (corpus-tools/reference-libs/openigtlink-upstream) behind the same
# stdin-hex → stdout-JSON protocol as the other four oracle CLIs.
# Runs as a subprocess from `oigtl-corpus fuzz differential`, process-
# isolated so upstream crashes on malformed input (its readers predate
# sanitizers) don't take the fuzzer down.
#
# Build order:
#   1. Build upstream first:
#        cmake -S corpus-tools/reference-libs/openigtlink-upstream \
#              -B corpus-tools/reference-libs/openigtlink-upstream/build \
#              -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF \
#              -DBUILD_SHARED_LIBS=OFF
#        cmake --build .../openigtlink-upstream/build --parallel
#   2. Then this target:
#        cmake -S corpus-tools/reference-libs/upstream-oracle \
#              -B corpus-tools/reference-libs/upstream-oracle/build
#        cmake --build .../upstream-oracle/build --parallel
#
# The UPSTREAM_ROOT cache variable defaults to the sibling path but
# can be overridden if you've pointed upstream somewhere else.
cmake_minimum_required(VERSION 3.16)
project(oigtl_upstream_oracle CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(UPSTREAM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../openigtlink-upstream"
    CACHE PATH "Path to the upstream OpenIGTLink source tree")
set(UPSTREAM_BUILD "${UPSTREAM_ROOT}/build"
    CACHE PATH "Path to the upstream build directory (with libOpenIGTLink.a)")

if(NOT EXISTS "${UPSTREAM_BUILD}/lib/libOpenIGTLink.a")
    message(FATAL_ERROR
        "libOpenIGTLink.a not found under ${UPSTREAM_BUILD}/lib. "
        "Build upstream first; see the header of this CMakeLists.txt.")
endif()

foreach(pair
        "upstream_oracle_cli;oracle_cli.cpp"
        "upstream_generator_cli;upstream_generator_cli.cpp")
    list(GET pair 0 target)
    list(GET pair 1 source)
    add_executable(${target} ${source})
    target_include_directories(${target} PRIVATE
        "${UPSTREAM_ROOT}/Source"
        "${UPSTREAM_ROOT}/Source/igtlutil"
        "${UPSTREAM_BUILD}/Source"
        "${UPSTREAM_BUILD}"
    )
    target_link_libraries(${target} PRIVATE
        "${UPSTREAM_BUILD}/lib/libOpenIGTLink.a"
        "${UPSTREAM_BUILD}/lib/libigtlutil.a"
    )
endforeach()
