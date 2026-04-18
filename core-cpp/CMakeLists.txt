# core-cpp — Phase 1: hand-written runtime + reference TRANSFORM
#
# Targets:
#   oigtl_runtime  static lib, hand-written runtime (crc, header, ...)
#   oigtl_messages static lib, message codecs (Phase 1: TRANSFORM only)
#   oigtl          INTERFACE umbrella for downstream consumers
#   oigtl_tests    test binary (Phase 1: upstream TRANSFORM round-trip)
#
# Phase-2+ work will replace src/messages/*.cpp with codegen output.

cmake_minimum_required(VERSION 3.16)
project(oigtl_core CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Strict warnings — we want codegen output to compile cleanly.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror
                        -Wno-unused-parameter)
elseif(MSVC)
    add_compile_options(/W4 /WX)
endif()

# ---------------------------------------------------------------------------
# Runtime library — stdlib only, no external deps
# ---------------------------------------------------------------------------
add_library(oigtl_runtime STATIC
    src/runtime/crc64.cpp
    src/runtime/dispatch.cpp
    src/runtime/extended_header.cpp
    src/runtime/header.cpp
    src/runtime/metadata.cpp
    src/runtime/oracle.cpp
)
target_include_directories(oigtl_runtime PUBLIC include)

# ---------------------------------------------------------------------------
# Transport library — async Connection + Framer.
#   Phase 1: Future primitive, Framer interface + v3, loopback.
#   Phase 2: TCP backend via standalone asio (FetchContent).
#   Phase 3: TLS wrapper (mbedTLS).
# ---------------------------------------------------------------------------

# Fetch standalone asio. SOURCE_SUBDIR pointing at a non-existent path
# keeps FetchContent_MakeAvailable from running asio's own CMakeLists
# (we only want the headers).
include(FetchContent)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  _disabled_cmake_subdir
)
FetchContent_MakeAvailable(asio)
add_library(oigtl_asio_headers INTERFACE)
target_include_directories(oigtl_asio_headers
    SYSTEM INTERFACE "${asio_SOURCE_DIR}/asio/include")
target_compile_definitions(oigtl_asio_headers INTERFACE
    ASIO_STANDALONE ASIO_NO_DEPRECATED)

add_library(oigtl_transport STATIC
    src/transport/framer_v3.cpp
    src/transport/connection_loopback.cpp
    src/transport/io_runtime.cpp
    src/transport/connection_tcp.cpp
)
target_include_directories(oigtl_transport PUBLIC include)
target_link_libraries(oigtl_transport PUBLIC oigtl_runtime)
# asio is a private dep — no asio types leak through public headers.
target_link_libraries(oigtl_transport PRIVATE oigtl_asio_headers)
# Future<T> uses condition variables; pthread on Linux.
find_package(Threads REQUIRED)
target_link_libraries(oigtl_transport PUBLIC Threads::Threads)

# ---------------------------------------------------------------------------
# Message library
# ---------------------------------------------------------------------------
# Glob the generated message sources. CONFIGURE_DEPENDS makes CMake
# rerun globbing on each build, so adding a new schema and
# regenerating doesn't require editing this file.
file(GLOB OIGTL_MESSAGE_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/messages/*.cpp")
add_library(oigtl_messages STATIC ${OIGTL_MESSAGE_SOURCES})
target_link_libraries(oigtl_messages PUBLIC oigtl_runtime)

# ---------------------------------------------------------------------------
# Umbrella target for downstream consumers
# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------
# Ergonomic client/server facade — the "lovely API" that sits on top
# of transport + codec. Sync-only, templated on message types.
# ---------------------------------------------------------------------------
add_library(oigtl_ergo STATIC
    src/envelope.cpp
    src/client.cpp
    src/server.cpp
)
target_include_directories(oigtl_ergo PUBLIC include)
target_link_libraries(oigtl_ergo PUBLIC oigtl_runtime oigtl_transport)

add_library(oigtl INTERFACE)
target_link_libraries(oigtl INTERFACE
    oigtl_runtime oigtl_messages oigtl_transport oigtl_ergo)

# ---------------------------------------------------------------------------
# Upstream-compatibility shim — libigtl_compat.
#   Phase 5: LightObject + SmartPointer + Object + macros.
#   Phase 6: message facades (codegen-emitted).
#   Phase 7: ClientSocket / ServerSocket / Socket over oigtl_transport.
# Consumers of the upstream API (Slicer, PLUS, …) link against this
# library plus oigtl_transport + oigtl_messages + oigtl_runtime, no
# source changes required.
# ---------------------------------------------------------------------------
add_library(igtl_compat STATIC
    compat/src/igtlLightObject.cxx
    compat/src/igtlObject.cxx
    compat/src/igtlMath.cxx
    compat/src/igtlMessageBase.cxx
    compat/src/igtlTransformMessage.cxx
    compat/src/igtlStatusMessage.cxx
    compat/src/igtlStringMessage.cxx
    compat/src/igtlPositionMessage.cxx
    compat/src/igtlPointMessage.cxx
    compat/src/igtlTrackingDataMessage.cxx
    compat/src/igtlQuaternionTrackingDataMessage.cxx
    compat/src/igtlImageMessage.cxx
    compat/src/igtlCapabilityMessage.cxx
    compat/src/igtlSensorMessage.cxx
    compat/src/igtlCommandMessage.cxx
    compat/src/igtlQueryMessage.cxx
    compat/src/igtlTimeStamp.cxx
    compat/src/igtlImageMetaMessage.cxx
    compat/src/igtlLabelMetaMessage.cxx
    compat/src/igtlTrajectoryMessage.cxx
    compat/src/igtlNDArrayMessage.cxx
    compat/src/igtlColorTableMessage.cxx
    compat/src/igtlPolyDataMessage.cxx
    compat/src/igtlBindMessage.cxx
    compat/src/igtlSocket.cxx
    compat/src/igtlClientSocket.cxx
    compat/src/igtlServerSocket.cxx
)
target_include_directories(igtl_compat PUBLIC
    compat/include           # our sources use "igtl/igtlFoo.h"
    compat/include/igtl)     # upstream code uses bare "igtlFoo.h"
# Upstream examples / consumers gate v2+ includes behind the
# protocol-version macro. Our shim supports v3 — advertise it so
# consumer code picks up the full type set.
target_compile_definitions(igtl_compat PUBLIC
    OpenIGTLink_PROTOCOL_VERSION=3)
# Pack/Unpack in the facades goes through our runtime codec.
# Socket / ClientSocket / ServerSocket delegate to oigtl_transport.
target_link_libraries(igtl_compat PUBLIC oigtl_runtime oigtl_transport)
# Upstream's igtlTypeMacro / igtlNewMacro are typically called with
# a trailing `;` in consumer headers ("igtlTypeMacro(Foo, Bar);").
# That produces -Wextra-semi diagnostics. For drop-in compatibility
# we suppress that warning both when building the shim and when
# consumers include its headers.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(igtl_compat PUBLIC -Wno-extra-semi)
endif()
# Phase 5 has no transport / codec deps yet; added in Phase 6/7.

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()

set(IGTL_UPSTREAM_FIXTURES
    "${CMAKE_CURRENT_SOURCE_DIR}/../corpus-tools/reference-libs/openigtlink-upstream/Testing/igtlutil"
    CACHE PATH "Path to upstream igtl test-data headers")

add_executable(oigtl_tests
    tests/upstream_fixtures_test.cpp
)
target_link_libraries(oigtl_tests PRIVATE oigtl)
target_include_directories(oigtl_tests PRIVATE
    "${IGTL_UPSTREAM_FIXTURES}")

add_test(NAME upstream_fixtures COMMAND oigtl_tests)

# Cross-language parity test — shells out to the Python oracle via
# `uv run oigtl-corpus oracle verify --fixture ...` for every
# upstream fixture and asserts agreement. Skips at runtime if `uv`
# isn't on PATH, so it doesn't fail in environments without the
# Python tooling installed.
add_executable(oigtl_oracle_parity_test
    tests/oracle_parity_test.cpp
)
target_link_libraries(oigtl_oracle_parity_test PRIVATE oigtl)
target_include_directories(oigtl_oracle_parity_test PRIVATE
    "${IGTL_UPSTREAM_FIXTURES}")
add_test(NAME oracle_parity COMMAND oigtl_oracle_parity_test)
# The parity test invokes the corpus-tools CLI from a fixed
# location relative to the source tree; force the working
# directory so the relative path is stable regardless of where
# ctest is invoked from.
set_tests_properties(oracle_parity PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../corpus-tools")

# Negative corpus — every must-reject entry must be rejected by
# the C++ oracle. Loads spec/corpus/negative/index.json at runtime.
add_executable(oigtl_negative_corpus_test
    tests/negative_corpus_test.cpp
)
target_link_libraries(oigtl_negative_corpus_test PRIVATE oigtl)
add_test(NAME negative_corpus COMMAND oigtl_negative_corpus_test)
set_tests_properties(negative_corpus PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# Transport Phase 1 — loopback Connection + Framer + Future contract.
add_executable(oigtl_transport_loopback_test
    tests/transport_loopback_test.cpp
)
target_link_libraries(oigtl_transport_loopback_test
    PRIVATE oigtl_transport oigtl_runtime)
add_test(NAME transport_loopback COMMAND oigtl_transport_loopback_test)

# Transport Phase 2 — TCP backend over asio.
add_executable(oigtl_transport_tcp_test
    tests/transport_tcp_test.cpp
)
target_link_libraries(oigtl_transport_tcp_test
    PRIVATE oigtl_transport oigtl_runtime)
add_test(NAME transport_tcp COMMAND oigtl_transport_tcp_test)
set_tests_properties(transport_tcp PROPERTIES TIMEOUT 60)

# Transport Phase 4 — sync::block_on bridge for blocking callers.
add_executable(oigtl_transport_sync_test
    tests/transport_sync_test.cpp
)
target_link_libraries(oigtl_transport_sync_test
    PRIVATE oigtl_transport oigtl_runtime)
add_test(NAME transport_sync COMMAND oigtl_transport_sync_test)

# Shim Phase 5 — LightObject + SmartPointer refcount semantics.
add_executable(igtl_compat_smartpointer_test
    compat/tests/smartpointer_test.cxx
)
target_link_libraries(igtl_compat_smartpointer_test
    PRIVATE igtl_compat)
add_test(NAME compat_smartpointer
         COMMAND igtl_compat_smartpointer_test)

# Shim Phase 7 — Socket / ClientSocket / ServerSocket round-trip.
# Exercises the exact pattern every upstream example uses:
#   Receive(header_ptr, header_size); Unpack();
#   Receive(body_ptr, body_size);     Unpack();
add_executable(igtl_compat_socket_roundtrip_test
    compat/tests/socket_roundtrip_test.cxx
)
target_link_libraries(igtl_compat_socket_roundtrip_test
    PRIVATE igtl_compat)
add_test(NAME compat_socket_roundtrip
         COMMAND igtl_compat_socket_roundtrip_test)
set_tests_properties(compat_socket_roundtrip PROPERTIES TIMEOUT 30)

# -----------------------------------------------------------------
# Phase 8 — upstream example programs built DIRECTLY against the
# shim (no upstream library in the link). Proves the shim is a
# source-level drop-in: if TrackerServer.cxx compiles and runs
# against igtl_compat alone, every consumer that writes against
# the upstream API works.
#
# These programs live in the upstream source tree and we do NOT
# modify them — just point the compiler at them.
# -----------------------------------------------------------------
set(IGTL_UPSTREAM_EXAMPLES
    "${CMAKE_CURRENT_SOURCE_DIR}/../corpus-tools/reference-libs/openigtlink-upstream/Examples")

foreach(upstream_example
            Tracker/TrackerServer
            Tracker/TrackerClient
            Receiver/ReceiveServer
            Receiver/ReceiveClient
            Status/StatusServer
            Status/StatusClient)
    get_filename_component(_prog_name "${upstream_example}" NAME)
    set(_src "${IGTL_UPSTREAM_EXAMPLES}/${upstream_example}.cxx")
    if(EXISTS "${_src}")
        add_executable(upstream_${_prog_name}_via_shim "${_src}")
        target_link_libraries(upstream_${_prog_name}_via_shim
            PRIVATE igtl_compat)
        # Upstream code has legacy signed/unsigned comparisons etc.
        # We're NOT fixing upstream — just building it.
        target_compile_options(upstream_${_prog_name}_via_shim PRIVATE
            -Wno-error -Wno-unused-parameter -Wno-unused-variable
            -Wno-sign-compare -Wno-unused-but-set-variable)
    endif()
endforeach()

# Live interop gate — orchestrates unmodified upstream
# ReceiveServer.cxx and TrackerClient.cxx as subprocesses.
if(TARGET upstream_ReceiveServer_via_shim
   AND TARGET upstream_TrackerClient_via_shim)
    add_test(NAME compat_upstream_examples_interop
        COMMAND bash
            "${CMAKE_CURRENT_SOURCE_DIR}/compat/tests/upstream_examples_interop.sh"
            "$<TARGET_FILE:upstream_ReceiveServer_via_shim>"
            "$<TARGET_FILE:upstream_TrackerClient_via_shim>"
            "18956")
    set_tests_properties(compat_upstream_examples_interop
        PROPERTIES TIMEOUT 15)
endif()

# Byte-parity gate — two separate emitter binaries to avoid ODR
# collisions on `igtl::FooMessage` (our shim's class and upstream's
# class share the namespace; linking both in the same binary is
# undefined). Each emitter packs with canonical inputs and writes
# wire bytes to stdout; a ctest command diffs them.
if(EXISTS "${IGTL_UPSTREAM_BUILD}/lib/libOpenIGTLink.a")
    add_executable(compat_emitter_shim
        compat/tests/emitter_shim.cxx)
    target_link_libraries(compat_emitter_shim PRIVATE igtl_compat)

    add_executable(compat_emitter_upstream
        compat/tests/emitter_upstream.cxx)
    target_include_directories(compat_emitter_upstream PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../corpus-tools/reference-libs/openigtlink-upstream/Source"
        "${CMAKE_CURRENT_SOURCE_DIR}/../corpus-tools/reference-libs/openigtlink-upstream/Source/igtlutil"
        "${IGTL_UPSTREAM_BUILD}")
    target_link_libraries(compat_emitter_upstream PRIVATE
        "${IGTL_UPSTREAM_BUILD}/lib/libOpenIGTLink.a")
    target_compile_options(compat_emitter_upstream PRIVATE
        -Wno-error -Wno-unused-parameter)

    # One ctest per parity case. Uses bash process-substitution
    # (requires bash, not plain sh).
    # STT_TDATA intentionally excluded — upstream's
    # StartTrackingDataMessage carries a body (resolution + coord_name)
    # that our codegen stub doesn't populate. It's a stub-vs-facade
    # difference, not a codec defect.
    foreach(case transform_v1 transform_v2 transform_v2_meta
                 transform_v2_rot90z get_status_v2 status_v2
                 string_v2 position_only_v2 position_quat3_v2
                 position_all_v2 point_v2 tdata_v2 qtdata_v2
                 image_v2 capability_v2 sensor_v2
                 command_v2 query_v2 imgmeta_v2 lbmeta_v2
                 traj_v2 ndarray_v2 colort_v2
                 polydata_empty_v2 polydata_tri_v2)
        add_test(NAME compat_parity_${case}
            COMMAND bash -c
                "cmp <(${CMAKE_CURRENT_BINARY_DIR}/compat_emitter_shim ${case}) <(${CMAKE_CURRENT_BINARY_DIR}/compat_emitter_upstream ${case})")
    endforeach()
endif()

# Ergonomic layer — Envelope, pack/unpack, Client, Server.
add_executable(oigtl_ergo_test
    tests/ergo_test.cpp
)
target_link_libraries(oigtl_ergo_test PRIVATE oigtl)
add_test(NAME ergo COMMAND oigtl_ergo_test)
set_tests_properties(ergo PROPERTIES TIMEOUT 30)

# Oracle CLI — persistent subprocess for `oigtl-corpus fuzz differential`.
# Reads hex-encoded wire bytes from stdin, emits JSON reports on stdout.
# Not a test target; built but never invoked by ctest.
add_executable(oigtl_oracle_cli
    src/oracle_cli.cpp
)
target_link_libraries(oigtl_oracle_cli PRIVATE oigtl)

# ---------------------------------------------------------------------------
# libFuzzer memory-safety gate (Phase 3 of security/PLAN.md)
# ---------------------------------------------------------------------------
# Opt-in: configure with `-DBUILD_FUZZERS=ON` on a Clang toolchain.
# Requires Clang's libFuzzer support; GCC is not supported (no
# drop-in replacement for -fsanitize=fuzzer).
#
# Builds two entry points:
#   fuzz_header  — drives unpack_header + verify_crc
#   fuzz_oracle  — drives verify_wire_bytes (full pipeline)
#
# ASan + UBSan are always enabled for fuzz builds — the whole point
# is to catch memory-safety and undefined-behaviour issues. A bug
# that libFuzzer surfaces without sanitizers is usually invisible
# (silent OOB read → identical JSON output → no disagreement).
option(BUILD_FUZZERS
       "Build libFuzzer entry points under fuzz/ (requires Clang)"
       OFF)

if(BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR
            "BUILD_FUZZERS=ON requires Clang (found "
            "${CMAKE_CXX_COMPILER_ID}). Re-run cmake with "
            "-DCMAKE_CXX_COMPILER=clang++.")
    endif()

    set(OIGTL_FUZZ_FLAGS
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -g)

    # Homebrew LLVM ships libFuzzer runtime linked against its own
    # libc++ — we must match or the link fails with missing
    # std::__1 symbols. Detect a non-system clang and rewire.
    set(OIGTL_FUZZ_LINK_FLAGS ${OIGTL_FUZZ_FLAGS})
    if(CMAKE_CXX_COMPILER MATCHES "/opt/homebrew/opt/llvm|/usr/local/opt/llvm")
        get_filename_component(_llvm_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(_llvm_root "${_llvm_bin}" DIRECTORY)
        list(APPEND OIGTL_FUZZ_LINK_FLAGS
            -L${_llvm_root}/lib/c++
            -Wl,-rpath,${_llvm_root}/lib/c++)
    endif()

    foreach(target fuzz_header fuzz_oracle)
        add_executable(${target} fuzz/${target}.cc)
        target_link_libraries(${target} PRIVATE oigtl)
        target_compile_options(${target} PRIVATE ${OIGTL_FUZZ_FLAGS})
        target_link_options(${target} PRIVATE ${OIGTL_FUZZ_LINK_FLAGS})
    endforeach()

    # fuzz_compat drives the compat-shim's hand-written UnpackContent()
    # parsers. Separate from fuzz_oracle because the shim has its own
    # parsing code that does NOT share with oigtl_messages.
    add_executable(fuzz_compat fuzz/fuzz_compat.cc)
    target_link_libraries(fuzz_compat PRIVATE igtl_compat oigtl_runtime)
    target_compile_options(fuzz_compat PRIVATE ${OIGTL_FUZZ_FLAGS})
    target_link_options(fuzz_compat PRIVATE ${OIGTL_FUZZ_LINK_FLAGS})

    # Convenience: a target that copies the on-disk negative
    # corpus + upstream fixtures into a seed-corpus directory the
    # fuzzers can consume directly. LibFuzzer recurses, so the
    # generator is free to lay out subfolders.
    add_custom_target(fuzz_seed_corpus
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/seeds/header"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/seeds/oracle"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/../spec/corpus/negative"
            "${CMAKE_CURRENT_BINARY_DIR}/seeds/oracle"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/../spec/corpus/negative/framing_header"
            "${CMAKE_CURRENT_BINARY_DIR}/seeds/header"
        COMMENT "Staging fuzz seed corpus under build/seeds/"
        VERBATIM)
endif()

# Benchmark binary — built but not run by ctest. Invoke manually:
#   cmake --build build --target oigtl_bench && ./build/oigtl_bench
# Use a Release build for representative numbers:
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
add_executable(oigtl_bench
    benches/bench_codec.cpp
)

# ---------------------------------------------------------------------------
# Examples — runnable programs demonstrating the ergonomic API.
# ---------------------------------------------------------------------------
add_executable(tracker_pair
    examples/tracker_pair.cpp
)
target_link_libraries(tracker_pair PRIVATE oigtl)

# Transport micro-benchmark — TRANSFORM throughput through our API.
add_executable(oigtl_bench_transport_ours
    benches/bench_transport_ours.cpp
)
target_link_libraries(oigtl_bench_transport_ours PRIVATE oigtl)

# Optional: transport micro-benchmark via upstream's ClientSocket /
# ServerSocket for apples-to-apples comparison. Requires upstream
# already built at corpus-tools/reference-libs/openigtlink-upstream.
set(IGTL_UPSTREAM_BUILD
    "${CMAKE_CURRENT_SOURCE_DIR}/../corpus-tools/reference-libs/openigtlink-upstream/build"
    CACHE PATH "Path to upstream OpenIGTLink build directory")
if(EXISTS "${IGTL_UPSTREAM_BUILD}/lib/libOpenIGTLink.a")
    add_executable(oigtl_bench_transport_upstream
        benches/bench_transport_upstream.cpp
    )
    target_include_directories(oigtl_bench_transport_upstream PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../corpus-tools/reference-libs/openigtlink-upstream/Source"
        "${CMAKE_CURRENT_SOURCE_DIR}/../corpus-tools/reference-libs/openigtlink-upstream/Source/igtlutil"
        "${IGTL_UPSTREAM_BUILD}"
    )
    target_link_libraries(oigtl_bench_transport_upstream PRIVATE
        "${IGTL_UPSTREAM_BUILD}/lib/libOpenIGTLink.a")
    # Suppress -Werror: upstream header has legacy signed/unsigned
    # comparisons and we are not going to patch it.
    target_compile_options(oigtl_bench_transport_upstream PRIVATE
        -Wno-error -Wno-unused-parameter)
endif()
target_link_libraries(oigtl_bench PRIVATE oigtl)
target_include_directories(oigtl_bench PRIVATE
    "${IGTL_UPSTREAM_FIXTURES}")
