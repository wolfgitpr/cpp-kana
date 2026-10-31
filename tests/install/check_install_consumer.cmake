# Install consumer gate: install to a throwaway prefix, then consume it the way a stranger would.
#
# The reason this is a gate and not a paragraph in a README: an install rule and a package config are the
# only parts of a build that no test in the build tree ever exercises. `cmake --install` can succeed while
#   * the Config file names a target that was never exported,
#   * the .pc file's Libs line omits the C++ runtime the consumer now needs,
#   * find_package accepts a version it should refuse.
# Each of those ships a package that installs cleanly and fails on the user's machine.
#
# Invoked by ctest as:
#   cmake -DSOURCE_DIR=<repo> -DINSTALL_BUILD_DIR=<build> -DWORK_DIR=<tmp> -P check_install_consumer.cmake
#
# It installs from the EXISTING build tree (no rebuild), then builds two consumer projects with the parent
# generator: one that must compile and run, and one that asks for version 3.0 and must be refused.

cmake_minimum_required(VERSION 3.20)

foreach(REQUIRED IN ITEMS SOURCE_DIR INSTALL_BUILD_DIR WORK_DIR)
    if(NOT DEFINED ${REQUIRED})
        message(FATAL_ERROR "check_install_consumer: ${REQUIRED} is not set")
    endif()
endforeach()

set(PREFIX "${WORK_DIR}/prefix")
set(CONSUMER "${WORK_DIR}/consumer")
set(REFUSED "${WORK_DIR}/refused")
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${PREFIX}" "${CONSUMER}" "${REFUSED}")

# --- 1. install -----------------------------------------------------------------------------------
execute_process(
        COMMAND "${CMAKE_COMMAND}" --install "${INSTALL_BUILD_DIR}" --prefix "${PREFIX}"
        RESULT_VARIABLE INSTALL_STATUS OUTPUT_VARIABLE INSTALL_OUTPUT ERROR_VARIABLE INSTALL_OUTPUT)
if(NOT INSTALL_STATUS EQUAL 0)
    message(FATAL_ERROR "check_install_consumer: cmake --install failed:\n${INSTALL_OUTPUT}")
endif()

# --- 2. the tree must contain what the package promises --------------------------------------------
set(MISSING "")
file(GLOB CONFIG_FILES "${PREFIX}/*/cmake/cpp-kana2/cpp-kana2Config.cmake"
        "${PREFIX}/lib/cmake/cpp-kana2/cpp-kana2Config.cmake"
        "${PREFIX}/share/cmake/cpp-kana2/cpp-kana2Config.cmake")
if(NOT CONFIG_FILES)
    string(APPEND MISSING "  no cpp-kana2Config.cmake in ${PREFIX} (a consumer cannot find_package it)\n")
endif()
file(GLOB PACKAGE_FILES "${PREFIX}/lib*/pkgconfig/cpp-kana2.pc")
if(NOT PACKAGE_FILES)
    string(APPEND MISSING "  no cpp-kana2.pc (a Makefile consumer has nothing to ask pkg-config for)\n")
endif()
if(NOT EXISTS "${PREFIX}/include/cpp-kana2/core/Convert.h")
    string(APPEND MISSING "  include/cpp-kana2/core/Convert.h is not installed\n")
endif()
if(NOT EXISTS "${PREFIX}/include/cpp-kana2/ext/c_api.h")
    string(APPEND MISSING "  include/cpp-kana2/ext/c_api.h is not installed (the C ABI is part of the package)\n")
endif()
if(MISSING)
    message(FATAL_ERROR "check_install_consumer: the installed tree is incomplete\n${MISSING}")
endif()

# --- 3. a consumer that must work ------------------------------------------------------------------
file(WRITE "${CONSUMER}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.20)
project(consumer LANGUAGES CXX)
find_package(cpp-kana2 2.0 CONFIG REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE cpp-kana2::cpp-kana2)
]=])
file(WRITE "${CONSUMER}/main.cpp" [=[
#include <cpp-kana2/core/Convert.h>
#include <cstdio>
#include <string>

int main() {
    const auto result = cpp_kana::v2::kanaToRomaji("きって");
    if (!result.ok()) {
        std::fprintf(stderr, "conversion failed\n");
        return 1;
    }
    const std::string rendered = result.value().rendered();
    std::printf("%s\n", rendered.c_str());
    return rendered == "kitte" ? 0 : 2;
}
]=])

# The compiler and generator are forwarded: a nested configure does not inherit them, and without them the
# consumer project fails with "CMAKE_CXX_COMPILER not set" -- a red gate about the wrong thing.
set(CONSUMER_GENERATOR_ARGS "")
if(DEFINED GENERATOR AND NOT GENERATOR STREQUAL "")
    list(APPEND CONSUMER_GENERATOR_ARGS -G "${GENERATOR}")
endif()
set(CONSUMER_COMPILER_ARGS "")
if(DEFINED COMPILER AND NOT COMPILER STREQUAL "")
    list(APPEND CONSUMER_COMPILER_ARGS "-DCMAKE_CXX_COMPILER=${COMPILER}")
endif()
# The outer build's flags are forwarded too. MEASURED: a Clang build that targets the MinGW runtime needs
# "--target=x86_64-w64-windows-gnu -idirafter <sysroot>/include" in CMAKE_CXX_FLAGS; without it the consumer
# is configured for the MSVC target and fails to link, reporting a broken install that is not broken.
#
# Sanitizer flags are the exception, and they are stripped: they describe the code under test, not the
# consumer. A consumer that inherits -fsanitize=address,undefined from an instrumented build links against a
# library that is NOT instrumented and, on the ASan leg of CI, dies in the linker or at startup -- turning this
# gate red for a reason that has nothing to do with the installed package. MEASURED: that is exactly what
# happened on the first real CI run (Gate_InstallConsumer, 0.56s).
if(DEFINED CXX_FLAGS AND NOT CXX_FLAGS STREQUAL "")
    # Sanitizer flags travel WITH the toolchain flags, and that is the correction of an earlier mistake: I
    # stripped them first, measured the installed library still instrumented, and the consumer then died at load
    # time with "ASan runtime does not come first in initial library list" -- the consumer must be built with the
    # sanitizer the library it loads was built with. The failure the stripping was meant to fix was the same one.
    # The library under test may be instrumented while CMAKE_CXX_FLAGS carries no sanitizer flag (the sanitizer
    # travels on the cpp-kana2_sanitizers interface target), so the caller appends what a consumer must link to
    # load such a library. Empty in a normal build.
    list(APPEND CONSUMER_COMPILER_ARGS "-DCMAKE_CXX_FLAGS=${CXX_FLAGS} ${CPP_KANA2_CONSUMER_SANITIZER_FLAGS}")
endif()
if(DEFINED EXE_LINK_FLAGS AND NOT EXE_LINK_FLAGS STREQUAL "")
    list(APPEND CONSUMER_COMPILER_ARGS "-DCMAKE_EXE_LINKER_FLAGS=${EXE_LINK_FLAGS}")
endif()

execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${CONSUMER}" -B "${CONSUMER}/build"
                ${CONSUMER_GENERATOR_ARGS} ${CONSUMER_COMPILER_ARGS}
                "-DCMAKE_PREFIX_PATH=${PREFIX}" "-DCMAKE_BUILD_TYPE=Release"
        RESULT_VARIABLE CONFIGURE_STATUS OUTPUT_VARIABLE CONFIGURE_OUTPUT ERROR_VARIABLE CONFIGURE_OUTPUT)
if(NOT CONFIGURE_STATUS EQUAL 0)
    message(FATAL_ERROR "check_install_consumer: the consumer could not find the package:\n${CONFIGURE_OUTPUT}")
endif()

execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${CONSUMER}/build"
        RESULT_VARIABLE BUILD_STATUS OUTPUT_VARIABLE BUILD_OUTPUT ERROR_VARIABLE BUILD_OUTPUT)
if(NOT BUILD_STATUS EQUAL 0)
    message(FATAL_ERROR "check_install_consumer: the consumer could not build against the installed package:\n"
            "${BUILD_OUTPUT}")
endif()

# The consumer's executable needs the shared library at run time, and the install prefix is not on the
# loader's search path. Running through "cmake -E env" with the prefix prepended is what a user following
# the README would do; the DLL-not-found case (0xC0000135) is otherwise reported as "did not run correctly".
if(WIN32)
    set(SEPARATOR ";")
    set(RUNTIME_DIRS "${PREFIX}/bin;${PREFIX}/lib")
else()
    set(SEPARATOR ":")
    set(RUNTIME_DIRS "${PREFIX}/lib:${PREFIX}/lib64")
endif()
execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "PATH=${RUNTIME_DIRS}${SEPARATOR}$ENV{PATH}"
                # A sanitizer build installs an INSTRUMENTED library, and a consumer that is built and linked
                # with the sanitizer still fails ASan's link-order check when the runtime is not the first
                # entry in the process's library list -- the normal outcome for a ctest child that loads the
                # library through its rpath. verify_asan_link_order exists for exactly that arrangement;
                # without it the gate reports a broken install that is not broken. MEASURED on the sanitize
                # leg: "ASan runtime does not come first in initial library list", three runs in a row.
                "ASAN_OPTIONS=verify_asan_link_order=0:detect_leaks=0"
                "${CONSUMER}/build/consumer"
        RESULT_VARIABLE RUN_STATUS OUTPUT_VARIABLE RUN_OUTPUT ERROR_VARIABLE RUN_OUTPUT)
if(NOT RUN_STATUS EQUAL 0)
    message(FATAL_ERROR "check_install_consumer: the consumer built but did not run correctly "
            "(status ${RUN_STATUS}, output '${RUN_OUTPUT}')")
endif()

# --- 4. a consumer asking for a version that must be refused ----------------------------------------
#
# The request is 1.0, NOT 3.0, and the difference is the whole point: every compatibility mode refuses a
# request for a NEWER major, so 3.0 is refused even by AnyNewerVersion -- the exact misconfiguration this
# section exists to catch (CMakeLists.txt sets SameMajorVersion). 1.0 is refused only when the major version
# is really compared, so a regression to AnyNewerVersion turns this red.
file(WRITE "${REFUSED}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.20)
project(refused LANGUAGES CXX)
find_package(cpp-kana2 1.0 CONFIG REQUIRED)
]=])

execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${REFUSED}" -B "${REFUSED}/build"
                ${CONSUMER_GENERATOR_ARGS} ${CONSUMER_COMPILER_ARGS} "-DCMAKE_PREFIX_PATH=${PREFIX}"
        RESULT_VARIABLE REFUSE_STATUS OUTPUT_VARIABLE REFUSE_OUTPUT ERROR_VARIABLE REFUSE_OUTPUT)
if(REFUSE_STATUS EQUAL 0)
    message(FATAL_ERROR "check_install_consumer: find_package(cpp-kana2 1.0) was ACCEPTED; the version "
            "compatibility rule is not being applied (SameMajorVersion is what makes 2.0 refuse 1.0)")
endif()
# A non-zero status alone is not evidence of a version refusal: a missing compiler or an empty prefix fails
# the same way. CMake's own wording is the discriminator.
if(NOT REFUSE_OUTPUT MATCHES "compatible with requested version|could not find a package configuration")
    message(FATAL_ERROR "check_install_consumer: the 1.0 consumer failed for a reason other than the version "
            "check, so this gate proved nothing:\n${REFUSE_OUTPUT}")
endif()

message(STATUS "check_install_consumer: installed to ${PREFIX}; a consumer built and printed "
               "'${RUN_OUTPUT}'; find_package(cpp-kana2 1.0) was refused as it must be")
