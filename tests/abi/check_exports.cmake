# Export gate: what the shared library actually exports, measured from the binary.
#
# Why this cannot be a compile-time check: visibility attributes are a promise, and the export table is the
# evidence. The sibling project this one is modelled on shipped 269 exports from a library whose C++ surface
# was 16, because every std::__detail instantiation came along for the ride. A link-time promise does not
# catch that; objdump does.
#
# MEASURED, on the MinGW build this gate was written against: 25 exports = 9 cppk2_* + 16 C++ public names.
# On MSVC the mangled names differ but the rules below are about which symbols exist, not how they are
# spelled, so the same rules apply.
#
# Invoked by ctest as:
#   cmake -DLIBRARY=<file> -DEXPECTED=<tests/abi/expected_c_api_symbols.txt> -P check_exports.cmake

cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED LIBRARY OR NOT EXISTS "${LIBRARY}")
    message(FATAL_ERROR "check_exports: LIBRARY='${LIBRARY}' does not exist")
endif()
if(NOT DEFINED EXPECTED OR NOT EXISTS "${EXPECTED}")
    message(FATAL_ERROR "check_exports: EXPECTED='${EXPECTED}' does not exist")
endif()

get_filename_component(LIBRARY_NAME "${LIBRARY}" NAME)

file(STRINGS "${EXPECTED}" EXPECTED_LINES REGEX "^cppk2_")

set(EXPORTS "")
if(LIBRARY_NAME MATCHES "\\.dll$")
    find_program(OBJDUMP_EXECUTABLE objdump)
    if(NOT OBJDUMP_EXECUTABLE)
        message(FATAL_ERROR "check_exports: objdump not found. This gate reads the export table from the "
                "binary; skipping would report a pass for a measurement that never happened. Install binutils.")
    endif()
    execute_process(COMMAND "${OBJDUMP_EXECUTABLE}" -p "${LIBRARY}"
            OUTPUT_VARIABLE DUMP ERROR_VARIABLE DUMP_ERROR RESULT_VARIABLE DUMP_STATUS)
    if(NOT DUMP_STATUS EQUAL 0)
        message(FATAL_ERROR "check_exports: objdump -p failed: ${DUMP_ERROR}")
    endif()
    # The PE name table lists "\t[   0] +base[   1]  0000 <mangled name>": an ordinal column, a hint column,
    # then the name. Rather than unpick the columns, the C entry points are matched by their (unmangled)
    # names and the leak count by the mangled prefix the standard library always has.
    string(FIND "${DUMP}" "Ordinal/Name Pointer] Table" NAME_TABLE_AT)
    if(NAME_TABLE_AT EQUAL -1)
        message(FATAL_ERROR "check_exports: ${LIBRARY_NAME} has no name pointer table in its export directory")
    endif()
    string(SUBSTRING "${DUMP}" ${NAME_TABLE_AT} -1 NAME_TABLE)

    string(REGEX MATCHALL "cppk2_[a-z0-9_]+" EXPORTS "${NAME_TABLE}")
    string(REGEX MATCHALL "\n[ \t]+\\[[ \t]*[0-9]+\\]" EXPORT_ROWS "${NAME_TABLE}")
    list(LENGTH EXPORT_ROWS EXPORT_COUNT)
    string(REGEX MATCHALL "_ZNSt|_ZSt|__cxxabiv1|_ZN5cpp_[a-z]*5ext" STD_SYMBOLS "${NAME_TABLE}")
    list(LENGTH STD_SYMBOLS STD_COUNT)
else()
    # ELF and Mach-O: the dynamic symbol table is the export table.
    find_program(NM_EXECUTABLE nm)
    if(NOT NM_EXECUTABLE)
        message(FATAL_ERROR "check_exports: nm not found. This gate reads the dynamic symbol table from the "
                "binary; skipping would report a pass for a measurement that never happened. Install binutils.")
    endif()
    execute_process(COMMAND "${NM_EXECUTABLE}" -D --defined-only "${LIBRARY}"
            OUTPUT_VARIABLE DUMP ERROR_VARIABLE DUMP_ERROR RESULT_VARIABLE DUMP_STATUS)
    if(NOT DUMP_STATUS EQUAL 0)
        # Mach-O has no -D; fall back to the plain table.
        execute_process(COMMAND "${NM_EXECUTABLE}" -gU "${LIBRARY}"
                OUTPUT_VARIABLE DUMP ERROR_VARIABLE DUMP_ERROR RESULT_VARIABLE DUMP_STATUS)
    endif()
    if(NOT DUMP_STATUS EQUAL 0)
        message(FATAL_ERROR "check_exports: nm failed on ${LIBRARY_NAME}: ${DUMP_ERROR}")
    endif()
    string(REGEX MATCHALL "[^\n]+" LINES "${DUMP}")
    foreach(LINE IN LISTS LINES)
        if(LINE MATCHES "^[0-9a-fA-F]+ [A-Za-z] (.+)$")
            list(APPEND EXPORTS "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    list(LENGTH EXPORTS EXPORT_COUNT)
endif()

if(EXPORT_COUNT EQUAL 0)
    message(FATAL_ERROR "check_exports: ${LIBRARY_NAME} exports nothing -- the export decoration is wrong")
endif()

# Self-test: the check above is only worth its verdict if it can report a missing symbol. This runs the same
# loop against a name that cannot be exported, and fails the gate if the loop says "all present".
if(DEFINED SELF_TEST)
    set(FAKE_PROBLEMS "")
    foreach(SYMBOL IN ITEMS cppk2_definitely_not_an_exported_symbol)
        list(FIND EXPORTS "${SYMBOL}" FOUND_AT)
        if(FOUND_AT EQUAL -1)
            string(APPEND FAKE_PROBLEMS "  missing C ABI symbol: ${SYMBOL}\n")
        endif()
    endforeach()
    if(NOT FAKE_PROBLEMS)
        message(FATAL_ERROR "check_exports: SELF-TEST FAILED -- a symbol that is not exported was reported as present")
    endif()
    message(STATUS "check_exports: self-test passed (${LIBRARY_NAME} has ${EXPORT_COUNT} exports; a name that is "
                   "not exported is reported)")
    return()
endif()
set(PROBLEMS "")

# 1. Every frozen C entry point must be there, by name.
foreach(SYMBOL IN LISTS EXPECTED_LINES)
    string(STRIP "${SYMBOL}" SYMBOL)
    list(FIND EXPORTS "${SYMBOL}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        string(APPEND PROBLEMS "  missing C ABI symbol: ${SYMBOL}\n")
    endif()
endforeach()

# 2. Nothing from the standard library's internals: that is the leak this gate exists for.
#
# On PE the count came from the raw name table (the EXPORTS list there holds only the C names, by design);
# on ELF the EXPORTS list holds every exported symbol, so it can be counted directly.
if(NOT DEFINED STD_COUNT)
    set(STD_COUNT 0)
    foreach(SYMBOL IN LISTS EXPORTS)
        if(SYMBOL MATCHES "_ZNSt|_ZSt|__cxxabiv1|std::__")
            math(EXPR STD_COUNT "${STD_COUNT} + 1")
        endif()
    endforeach()
endif()
if(STD_COUNT GREATER 0)
    string(APPEND PROBLEMS "  ${STD_COUNT} standard-library internal symbol(s) exported (inlining leaked)\n")
endif()

# 3. A cap that is a smell test, not a target. v1 exported 269 symbols from a 16-name surface.
if(EXPORT_COUNT GREATER 64)
    string(APPEND PROBLEMS "  ${EXPORT_COUNT} exports; the frozen surface is 25 -- check for leaked internals\n")
endif()

if(PROBLEMS)
    message(FATAL_ERROR "check_exports: ${LIBRARY_NAME} failed\n${PROBLEMS}")
endif()

message(STATUS "check_exports: ${LIBRARY_NAME} exports ${EXPORT_COUNT} symbols, all ${EXPECTED_LINES} C entry "
               "points present, no std:: internals")
