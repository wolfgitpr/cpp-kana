cmake_minimum_required(VERSION 3.17)
# 2.0.0 is the first release of the v2 line; the number is the
# contract for its first release, not a bump of the 1.0.0.0 line. This line is the SINGLE AUTHORITY
# for the version -- every other manifest points here (docs/STABILITY.md section 1, checked by
# tools/check_version_tokens.py).
project(cpp-kana2 VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
# Require the standard instead of silently decaying to a lower one.
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------
# Build options
# ----------------------------------
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
# This is the project's test switch and the only one consulted. Python 3 is required when it is ON,
# because several gates are Python scripts (see tests/CMakeLists.txt).
option(CPP_KANA2_BUILD_TESTS "Build test cases (requires a Python 3 interpreter)" OFF)
option(CPP_KANA2_BUILD_EXAMPLES "Build example programs" OFF)
# Sanitizers are a CI concern, not a user-facing build mode.
option(CPP_KANA2_SANITIZE "Build the library, tools and tests with ASan and UBSan (GCC/Clang)" OFF)
# ON only for a top-level build: when this project is embedded (FetchContent / add_subdirectory), the
# parent's `install` must not suddenly install this library, its headers and a .pc file.
set(_cpp_kana2_top_level OFF)
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_cpp_kana2_top_level ON)
endif ()
option(CPP_KANA2_INSTALL "Install library" ${_cpp_kana2_top_level})

# ----------------------------------
# CMake settings
# ----------------------------------
if (NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif ()

if (NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif ()

if (NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif ()

# ----------------------------------
# Reproducible artefacts
#
# MEASURED on this project family: two Release builds of the same commit produced archives with
# different SHA-256 while every object file inside was byte-identical, so the nondeterminism lives in
# the PACKAGING (ar stamps mtime/uid; the PE linker writes the build time into the DLL header).
# -ffile-prefix-map / /pathmap keep absolute build paths out of __FILE__ and debug info.
# ----------------------------------
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # CMAKE_CURRENT_SOURCE_DIR, not CMAKE_SOURCE_DIR: under add_subdirectory the latter is the
    # CONSUMER's root, and mapping their tree to "." would rewrite their `__FILE__` paths, not ours.
    add_compile_options(-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=. -ffile-prefix-map=${CMAKE_BINARY_DIR}=.)
    # Directory-scoped variables with no target-level equivalent: only set them when we own the build.
    if (_cpp_kana2_top_level)
        set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qcD <TARGET> <LINK_FLAGS> <OBJECTS>")
        set(CMAKE_CXX_ARCHIVE_APPEND "<CMAKE_AR> qD <TARGET> <LINK_FLAGS> <OBJECTS>")
        if (NOT APPLE)
            # `ar qcD` alone IS deterministic; the plain `ranlib` CMake runs afterwards is not, because
            # it rewrites the symbol index. Apple's ranlib has no -D and the archive is not shipped.
            set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -D <TARGET>")
        endif ()
    endif ()
endif ()
if (MINGW)
    # PE-linker option; passing it on ELF/Mach-O is an error, hence the guard.
    add_link_options(-Wl,--no-insert-timestamp)
endif ()
if (MSVC)
    add_compile_options(/Brepro /utf-8 /Zc:__cplusplus)
    add_link_options(/Brepro)
    # /pathmap is MSVC's counterpart of -ffile-prefix-map, and on its own it does NOT fail the build:
    # cl prints "requires /experimental:deterministic" once per TU and then ignores it. Keep the pair.
    add_compile_options("/experimental:deterministic" "/pathmap:${CMAKE_CURRENT_SOURCE_DIR}=.")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /manifest:no")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /manifest:no")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /manifest:no")
    if (NOT DEFINED CMAKE_DEBUG_POSTFIX)
        set(CMAKE_DEBUG_POSTFIX "d")
    endif ()
endif ()

if (CPP_KANA2_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
endif ()

# ----------------------------------
# Sources and headers
#
# EXPLICIT, NOT GLOB_RECURSE. A glob decides the build's content from whatever happens to be on disk: a
# stray file silently joins the library, and a deleted header keeps building from a stale copy.
# `tools/check_source_list.py` compares these lists with the tree, so adding a file and forgetting the
# list is red rather than silent.
# ----------------------------------
set(CPP_KANA2_SOURCES
    src/c_api/c_api.cpp
    src/core/Convert.cpp
    src/core/Mora.cpp
    src/data/Tables.cpp
    src/infra/CodeText.cpp
    src/infra/Unicode.cpp
    src/infra/Utf8.cpp
    src/mora/KanaToRomaji.cpp
    src/mora/Normalize.cpp
    src/mora/RomajiToKana.cpp
    src/mora/Segment.cpp)
set(CPP_KANA2_HEADERS
    include/cpp-kana2/core/Convert.h
    include/cpp-kana2/core/Global.h
    include/cpp-kana2/core/Mora.h
    include/cpp-kana2/core/Types.h
    include/cpp-kana2/core/Utf8.h
    include/cpp-kana2/core/Version.h
    include/cpp-kana2/ext/c_api.h
    src/data/TableData.inc
    src/data/Tables.h
    src/infra/Unicode.h
    src/mora/Internal.h)
set(_src ${CPP_KANA2_SOURCES} ${CPP_KANA2_HEADERS})

add_library(${PROJECT_NAME})
# SONAME: the shared library produces `libcpp-kana2.so.2` (plus the unversioned symlink). This is not a
# binary-compatibility promise -- same-major package/API compatibility only, rebuild for anything else.
# One number, PROJECT_VERSION_MAJOR, so a major bump moves the soname with the header/target/symbol
# tokens. PE has no soname; tests/abi/check_soname.cmake records that as n/a rather than skipping
# silently.
if (BUILD_SHARED_LIBS)
    set_target_properties(${PROJECT_NAME} PROPERTIES
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR})
endif ()
if (NOT BUILD_SHARED_LIBS)
    target_compile_definitions(${PROJECT_NAME} PUBLIC CPP_KANA2_STATIC)
    # A static install tree must stay linkable INTO a consumer's shared object; without PIC the
    # archive carries absolute-address relocations and `g++ -shared` on it fails at link time.
    set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif ()
target_compile_definitions(${PROJECT_NAME} PRIVATE
        CPP_KANA2_LIBRARY
        CPP_KANA2_VERSION_STRING="${PROJECT_VERSION}")

# ----------------------------------
# Symbol visibility
#
# The shared library has two kinds of consumers: C++ (find_package + link) and the C ABI (Python
# ctypes, C# P/Invoke). Both must see exactly what the headers mark with CPP_KANA2_EXPORT and nothing
# else. On MinGW there is no -fvisibility; the equivalent is --exclude-all-symbols, because GNU ld
# otherwise auto-exports every global symbol when no dllexport is present -- MEASURED on the v1 tree:
# its MinGW import library carried 1082 symbols, including std::__detail regex internals.
# ----------------------------------
if (BUILD_SHARED_LIBS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)
    if (MINGW)
        target_link_options(${PROJECT_NAME} PRIVATE -Wl,--exclude-all-symbols)
    elseif (UNIX AND NOT APPLE)
        # -fvisibility=hidden is not enough on ELF: templates instantiated in a translation unit that defines a
        # default-visibility class still come out default-visible. MEASURED on GCC 14 (Linux board replay):
        # 8 std:: internals were exported, and Gate_ExportGate caught them there for the first time -- the
        # Windows build never sees them because PE exports only what dllexport names. The version script is
        # the ELF counterpart of --exclude-all-symbols.
        target_link_options(${PROJECT_NAME} PRIVATE
                "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/cmake/exports.map")
        set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/exports.map")
    endif ()
endif ()

# ----------------------------------
# Warnings
#
# The library is warning-free under the strict set below. PRIVATE so consumers never inherit them.
# -Werror is an OPTION and not the default: the flags are private so they cannot break a consumer's
# build, and OFF-by-default keeps a newer compiler from failing someone's build; CI turns it ON.
# ----------------------------------
add_library(cpp-kana2_warnings INTERFACE)
option(CPP_KANA2_WERROR
       "Promote warnings to errors for the targets this build compiles (never inherited by consumers)" OFF)
if (CPP_KANA2_WERROR)
    if (MSVC)
        target_compile_options(cpp-kana2_warnings INTERFACE /WX)
    else ()
        target_compile_options(cpp-kana2_warnings INTERFACE -Werror)
    endif ()
endif ()
if (MSVC)
    target_compile_options(cpp-kana2_warnings INTERFACE /W4)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(cpp-kana2_warnings INTERFACE
            -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual
            -Wcast-qual -Wdouble-promotion -Wformat=2 -Wundef -Wredundant-decls
            -Wmisleading-indentation -Wextra-semi -Wlogical-op -Wsuggest-override
            -Wzero-as-null-pointer-constant)
    # NO -Wnull-dereference ON GCC: GCC reports "potential null pointer dereference" from inside
    # libstdc++'s own headers, which this tree cannot satisfy.
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(cpp-kana2_warnings INTERFACE
            -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual
            -Wcast-qual -Wdouble-promotion -Wformat=2 -Wundef -Wredundant-decls
            -Wnull-dereference)
endif ()

# ----------------------------------
# Sanitizers
#
# -fno-sanitize-recover=all, because without it UBSan prints and carries on, so a green run would mean
# nothing. The capability probe instantiates std::string on purpose: a trivial main can link while the
# sanitized STL cannot (measured: Windows clang wants stl_asan.lib and does not ship it).
# ----------------------------------
add_library(cpp-kana2_sanitizers INTERFACE)
if (CPP_KANA2_SANITIZE)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=address,undefined")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address,undefined")
    check_cxx_source_compiles("
        #include <string>
        int main() {
            std::string spelling = \"x\";
            return static_cast<int>(spelling.size()) - 1;
        }" CPP_KANA2_HAVE_SANITIZERS)
    unset(CMAKE_REQUIRED_FLAGS)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)
    if (NOT CPP_KANA2_HAVE_SANITIZERS)
        message(FATAL_ERROR
                "CPP_KANA2_SANITIZE=ON, but this compiler cannot build with -fsanitize=address,undefined. "
                "Build without the option, or use a toolchain that has the sanitizer runtimes.")
    endif ()
    target_compile_options(cpp-kana2_sanitizers INTERFACE
            -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g)
    target_link_options(cpp-kana2_sanitizers INTERFACE -fsanitize=address,undefined)
endif ()

# One name for "the flags every C++ target in this project builds with", so a new set is one line here
# instead of an edit at every target. C++ ONLY: the C API smoke test sets its own C flags.
add_library(cpp-kana2_dev_flags INTERFACE)
target_link_libraries(cpp-kana2_dev_flags INTERFACE cpp-kana2_warnings cpp-kana2_sanitizers)

target_sources(${PROJECT_NAME} PRIVATE ${_src})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} PRIVATE $<BUILD_INTERFACE:cpp-kana2_dev_flags>)

# Consumers must inherit C++17 through the exported target, otherwise a downstream project that
# defaults to C++14 (MSVC still does) fails on the headers.
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

if (CPP_KANA2_BUILD_TESTS)
    # The C ABI's smoke test is compiled as C on purpose (the header must work for a C compiler, and the
    # link must work from C). Enabling the C language only for the test build keeps a consumer's project,
    # which never compiles C here, free of the extra compiler lookup.
    enable_language(C)
    # enable_testing() has to happen in the ROOT scope: `include(CTest)` inside tests/ only generates
    # tests/CTestTestfile.cmake, so `ctest` from the build root reports "No tests were found".
    enable_testing()
    add_subdirectory(tests)
else ()
    # A switch that silently does nothing is worse than no switch: say what was not built.
    message(STATUS "cpp-kana2: CPP_KANA2_BUILD_TESTS=OFF -- no tests are configured in this build tree")
endif ()

if (CPP_KANA2_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

# ----------------------------------
# Platform-specific settings
# ----------------------------------
if (WIN32)
    set(RC_DESCRIPTION "A C++17 library for converting Japanese kana to romaji and back.")
    set(RC_COPYRIGHT "Copyright (C) 2023-2026 wolfgitpr")
    include("cmake/winrc.cmake")
endif ()

if (APPLE)
    # A deployment target that is too high does not fail locally: the binary links and runs on the
    # machine that built it and refuses to start on an older system. Declare the floor here so every
    # consumer (wheel, nupkg, find_package) inherits one answer.
    if (NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL "")
        if (CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
            set(_cpp_kana2_osx_floor "11.0")
        else ()
            set(_cpp_kana2_osx_floor "10.15")
        endif ()
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${_cpp_kana2_osx_floor}" CACHE STRING
            "Minimum macOS version (x86_64: 10.15; arm64: 11.0, the architecture floor)" FORCE)
    endif ()
    message(STATUS "macOS deployment target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif ()

# ----------------------------------
# Include directories
# ----------------------------------
target_include_directories(${PROJECT_NAME} PRIVATE include src)
target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# ----------------------------------
# Install settings
# ----------------------------------
if (CPP_KANA2_INSTALL)
    target_include_directories(${PROJECT_NAME} PUBLIC
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    install(TARGETS ${PROJECT_NAME}
            EXPORT ${PROJECT_NAME}Targets
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    )

    install(DIRECTORY include/${PROJECT_NAME}
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )

    # COMPATIBILITY SameMajorVersion, NOT AnyNewerVersion: the package promises API compatibility
    # inside a major version and never binary compatibility. Under AnyNewerVersion a request from the
    # v1 line was satisfied by a 2.x install, the configure succeeded, and the compile then failed on
    # headers that no longer exist. tests/install/check_install_consumer.cmake asserts a cross-major
    # request is REFUSED.
    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY SameMajorVersion
    )

    configure_package_config_file(
            ${CMAKE_CURRENT_LIST_DIR}/${PROJECT_NAME}Config.cmake.in
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
            INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
            NO_CHECK_REQUIRED_COMPONENTS_MACRO
    )

    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    )

    install(EXPORT ${PROJECT_NAME}Targets
            FILE ${PROJECT_NAME}Targets.cmake
            NAMESPACE ${PROJECT_NAME}::
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    )

    # The mora table ships with the library: a consumer that wants to reason about the table (or a
    # future one that loads an override) should not have to read the source. DESTINATION stays RELATIVE:
    # an absolute one is baked in at configure time and ignores `cmake --install --prefix`.
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/
            DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
    )
endif ()

# ----------------------------------
# pkg-config file
#
# Cflags has to say what the CMake target says: CPP_KANA2_STATIC is a PUBLIC compile definition (it
# selects the no-dllimport branch of Global.h), and the library needs C++17. Neither is inherited from
# a .pc the way it is from the CMake target, so both are written out here.
# ----------------------------------
if (CPP_KANA2_INSTALL)
    set(version ${PROJECT_VERSION})
    set(CPP_KANA2_PC_EXTRA_CFLAGS "")
    if (NOT BUILD_SHARED_LIBS)
        set(CPP_KANA2_PC_EXTRA_CFLAGS " -DCPP_KANA2_STATIC")
    endif ()

    configure_file(
            ${CMAKE_CURRENT_LIST_DIR}/cpp-kana2.pc.in
            ${CMAKE_BINARY_DIR}/cpp-kana2.pc
            @ONLY
    )

    install(
            FILES ${CMAKE_BINARY_DIR}/cpp-kana2.pc
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    )
endif ()
