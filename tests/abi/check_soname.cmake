# SONAME gate: on ELF, the shared library must announce its major version in its own dynamic section.
#
# Why it matters and why it is not automatic: the SONAME is what a consumer's executable records as "I need
# this". Without it, a binary built against 2.0 silently loads a future 3.0 with an incompatible ABI; with
# it, the loader refuses. CMake sets it from the target's VERSION/SOVERSION properties, and a property is a
# promise until the binary is read back -- which is what this gate does.
#
# On PE there is no SONAME (the import library carries the name), and on Mach-O the compatibility version
# plays that role; both are reported as skipped rather than passed silently.
#
# Invoked by ctest as:
#   cmake -DLIBRARY=<file> [-DEXPECTED_SONAME=...] [-DSELF_TEST=1] -P check_soname.cmake

cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED LIBRARY OR NOT EXISTS "${LIBRARY}")
    message(FATAL_ERROR "check_soname: LIBRARY='${LIBRARY}' does not exist")
endif()

get_filename_component(LIBRARY_NAME "${LIBRARY}" NAME)

if(LIBRARY_NAME MATCHES "\\.dll$")
    message(STATUS "check_soname: ${LIBRARY_NAME} is a PE image; PE has no SONAME, gate skipped")
    return()
endif()
if(LIBRARY_NAME MATCHES "\\.dylib$")
    message(STATUS "check_soname: ${LIBRARY_NAME} is a Mach-O image; its compatibility version is checked by "
                   "the platform tooling, gate skipped")
    return()
endif()

find_program(READELF_EXECUTABLE readelf)
if(NOT READELF_EXECUTABLE)
    message(FATAL_ERROR "check_soname: readelf not found. This gate is only registered on ELF, where readelf is "
            "standard; skipping would report a pass for a check that never ran. Install binutils.")
endif()

execute_process(COMMAND "${READELF_EXECUTABLE}" -d "${LIBRARY}"
        OUTPUT_VARIABLE DYNAMIC ERROR_VARIABLE DYNAMIC_ERROR RESULT_VARIABLE DYNAMIC_STATUS)
if(NOT DYNAMIC_STATUS EQUAL 0)
    message(FATAL_ERROR "check_soname: readelf -d failed on ${LIBRARY_NAME}: ${DYNAMIC_ERROR}")
endif()

if(NOT DYNAMIC MATCHES "\\(SONAME\\)[^\n]*\\[([^]]+)\\]")
    message(FATAL_ERROR "check_soname: ${LIBRARY_NAME} has no SONAME entry; a consumer would record the file "
            "path instead of the ABI name")
endif()
set(ACTUAL_SONAME "${CMAKE_MATCH_1}")

# The expected value comes from the build (SOVERSION = major), so it is one authority: the same number the
# version macros carry. The default below only exists so the script can run standalone.
if(NOT DEFINED EXPECTED_SONAME)
    set(EXPECTED_SONAME "libcpp-kana2.so.2")
endif()

if(NOT ACTUAL_SONAME STREQUAL EXPECTED_SONAME)
    message(FATAL_ERROR "check_soname: ${LIBRARY_NAME} announces SONAME '${ACTUAL_SONAME}', expected "
            "'${EXPECTED_SONAME}'")
endif()

message(STATUS "check_soname: ${LIBRARY_NAME} announces SONAME '${ACTUAL_SONAME}'")
