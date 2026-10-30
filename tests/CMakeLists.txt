# Tests for cpp-kana2. Registered from the root CMakeLists.txt with CPP_KANA2_BUILD_TESTS=ON.
project(cpp-kana2-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# Explicit source list, for the same reason the library has one: a glob would let a deleted test keep
# passing from a stale object file.
set(CPP_KANA2_TEST_SOURCES
    cpp/data_table.cpp
    cpp/smoke.cpp)

add_executable(cpp-kana2-tests ${CPP_KANA2_TEST_SOURCES})
target_include_directories(cpp-kana2-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cpp-kana2-tests PRIVATE cpp-kana2::cpp-kana2 cpp-kana2_dev_flags)

# The data tests read data/moras.tsv from the source tree: the table is data, and a test that used a
# copy of it would stop noticing when the two drift.
target_compile_definitions(cpp-kana2-tests PRIVATE
        CPP_KANA2_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

add_test(NAME Smoke COMMAND cpp-kana2-tests)
set_tests_properties(Smoke PROPERTIES LABELS "unit")

# ----------------------------------
# The C ABI, compiled as C
#
# Separate from the C++ suite because the header must work for a C compiler and the link must work from
# C: that is what the Python (ctypes) and C# (P/Invoke) shells depend on.
# ----------------------------------
add_executable(cpp-kana2-c-api-smoke c_api_smoke.c)
target_link_libraries(cpp-kana2-c-api-smoke PRIVATE cpp-kana2::cpp-kana2)
set_target_properties(cpp-kana2-c-api-smoke PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
if (MSVC)
    target_compile_options(cpp-kana2-c-api-smoke PRIVATE /W4)
else ()
    target_compile_options(cpp-kana2-c-api-smoke PRIVATE -Wall -Wextra -Wpedantic)
endif ()
# Sanitizers reach the C target directly: cpp-kana2_dev_flags carries C++-only warning flags, so linking
# it here would feed C++ flags to a C compiler.
target_link_libraries(cpp-kana2-c-api-smoke PRIVATE cpp-kana2_sanitizers)

add_test(NAME CapiSmoke COMMAND cpp-kana2-c-api-smoke)
set_tests_properties(CapiSmoke PROPERTIES LABELS "unit")

# ----------------------------------
# The conformance fixture
#
# This tool is the only writer of conformance/cases.json and tests/data/corpus/expected.tsv. It is built
# here rather than in the library so that a consumer never pays for it, and so that the fixture is
# derived from the same public API the shells use.
# ----------------------------------

add_executable(cpp-kana2-conformance ${CMAKE_SOURCE_DIR}/tools/conformance_emit.cpp)
target_link_libraries(cpp-kana2-conformance PRIVATE cpp-kana2::cpp-kana2)

add_test(NAME ConformanceFixture
        COMMAND cpp-kana2-conformance
                --corpus ${CMAKE_SOURCE_DIR}/tests/data/corpus/cases.tsv
                --dir ${CMAKE_SOURCE_DIR}
                --check)
set_tests_properties(ConformanceFixture PROPERTIES LABELS "unit")

# ----------------------------------
# Gates
#
# One ctest entry per gate, named after the script, so a failing gate is a failing test rather than a
# paragraph in a document. tools/check_gate_inventory.py compares this list with the registry in
# tools/run_all_gates.py in both directions.
#
# The gates are skipped, not failed, when no Python interpreter is found: the C++ suite must stay runnable
# on a machine that only has a compiler.
# ----------------------------------
find_package(Python3 COMPONENTS Interpreter QUIET)

set(CPP_KANA2_GATES
        check_source_list
        check_layers
        check_public_surface
        check_bindings
        check_version_tokens
        check_data_tables
        check_conformance
        check_doc_links
        check_doc_citations
        check_gate_inventory)

if (Python3_Interpreter_FOUND)
    foreach (gate IN LISTS CPP_KANA2_GATES)
        add_test(NAME Gate_${gate}
                COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/${gate}.py)
        set_tests_properties(Gate_${gate} PROPERTIES LABELS "gate")
    endforeach ()

    # Every gate proves it can fail before its verdict is read.
    add_test(NAME GateSelfTests COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/run_all_gates.py)
    set_tests_properties(GateSelfTests PROPERTIES LABELS "gate")
else ()
    message(STATUS "Python 3 not found: the gates are not registered as ctest tests")
endif ()

# ----------------------------------
# The export gate
#
# Measured from the shared library itself: visibility attributes are a promise, the export table is the
# evidence. Only meaningful for a shared build, and only for a target that exists.
# ----------------------------------
if (BUILD_SHARED_LIBS)
    add_test(NAME Gate_ExportGate
            COMMAND ${CMAKE_COMMAND}
                    -DLIBRARY=$<TARGET_FILE:cpp-kana2>
                    -DEXPECTED=${CMAKE_SOURCE_DIR}/tests/abi/expected_c_api_symbols.txt
                    -P ${CMAKE_SOURCE_DIR}/tests/abi/check_exports.cmake)
    add_test(NAME Gate_ExportGateSelfTest
            COMMAND ${CMAKE_COMMAND}
                    -DLIBRARY=$<TARGET_FILE:cpp-kana2>
                    -DEXPECTED=${CMAKE_SOURCE_DIR}/tests/abi/expected_c_api_symbols.txt
                    -DSELF_TEST=1
                    -P ${CMAKE_SOURCE_DIR}/tests/abi/check_exports.cmake)
    set_tests_properties(Gate_ExportGate Gate_ExportGateSelfTest PROPERTIES LABELS "gate")

    # ELF only: PE has no SONAME and Mach-O uses a compatibility version instead. The expected SONAME comes
    # from the same VERSION the package config advertises, so a mismatch is a real ABI statement.
    #
    # MEASURED BUG (found by the Linux board replay, invisible on Windows): this file declares its own
    # project(cpp-kana2-tests ...), so ${PROJECT_NAME} here is "cpp-kana2-tests" and the expectation came out
    # as "libcpp-kana2-tests.so." -- the gate failed while the library's SONAME was correct. CMAKE_PROJECT_NAME
    # and CMAKE_PROJECT_VERSION_MAJOR are the ROOT project's values in any subdirectory.
    if (UNIX AND NOT APPLE)
        add_test(NAME Gate_SonameGate
                COMMAND ${CMAKE_COMMAND}
                        -DLIBRARY=$<TARGET_FILE:cpp-kana2>
                        -DEXPECTED_SONAME=${CMAKE_SHARED_LIBRARY_PREFIX}${CMAKE_PROJECT_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${CMAKE_PROJECT_VERSION_MAJOR}
                        -P ${CMAKE_SOURCE_DIR}/tests/abi/check_soname.cmake)
        set_tests_properties(Gate_SonameGate PROPERTIES LABELS "gate")
    endif ()
endif ()

# ----------------------------------
# The install consumer
#
# Installs into a throwaway prefix from this build tree, then builds a consumer that must compile, link and
# print the right answer, and a second consumer asking for version 3.0 that must be refused. Cheap because
# it installs an existing build rather than rebuilding.
# ----------------------------------
if (CPP_KANA2_INSTALL)
    add_test(NAME Gate_InstallConsumer
            COMMAND ${CMAKE_COMMAND}
                    -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
                    -DINSTALL_BUILD_DIR=${CMAKE_BINARY_DIR}
                    -DWORK_DIR=${CMAKE_BINARY_DIR}/install-consumer
                    -DGENERATOR=${CMAKE_GENERATOR}
                    -DCOMPILER=${CMAKE_CXX_COMPILER}
                    -DCXX_FLAGS=${CMAKE_CXX_FLAGS}
                    -DEXE_LINK_FLAGS=${CMAKE_EXE_LINKER_FLAGS}
                    -P ${CMAKE_SOURCE_DIR}/tests/install/check_install_consumer.cmake)
    set_tests_properties(Gate_InstallConsumer PROPERTIES LABELS "gate")
endif ()
