# Windows version resource. Included from the root CMakeLists.txt only on WIN32.
#
# Two things here are load-bearing and were both wrong in the v1 file this was adapted from:
#
# 1. ${PROJECT_VERSION_TWEAK} is EMPTY for a three-component version (2.0.0), and an empty component
#    produces `FILEVERSION 2,0,0,` -- a syntax error that windres reports as "syntax error" on a line
#    nobody wrote by hand. The fourth component defaults to 0.
# 2. The strings are written with real quotes. v1 wrapped them in a STRINGIFY() macro, which cannot
#    take an argument containing spaces: "A lightweight library for converting ..." would have broken
#    the preprocessor, and the resource text would silently not be what the build declared.
if (NOT DEFINED PROJECT_VERSION_TWEAK OR PROJECT_VERSION_TWEAK STREQUAL "")
    set(_cpp_kana2_rc_tweak 0)
else ()
    set(_cpp_kana2_rc_tweak ${PROJECT_VERSION_TWEAK})
endif ()

set(_rc_content "#include <windows.h>

#ifndef VS_VERSION_INFO
#define VS_VERSION_INFO 1
#endif

VS_VERSION_INFO VERSIONINFO
    FILEVERSION    ${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},${_cpp_kana2_rc_tweak}
    PRODUCTVERSION ${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},${_cpp_kana2_rc_tweak}
{
    BLOCK \"StringFileInfo\"
    {
       // U.S. English - Windows, Multilingual
       BLOCK \"040904E4\"
       {
          VALUE \"FileDescription\", \"${RC_DESCRIPTION}\"
          VALUE \"FileVersion\", \"${PROJECT_VERSION}\"
          VALUE \"ProductName\", \"${PROJECT_NAME}\"
          VALUE \"ProductVersion\", \"${PROJECT_VERSION}\"
          VALUE \"LegalCopyright\", \"${RC_COPYRIGHT}\"
        }
    }
    BLOCK \"VarFileInfo\"
    {
        VALUE \"Translation\", 0x409, 1252 // 1252 = 0x04E4
    }
}")

set(_rc_file ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_res.rc)
file(WRITE ${_rc_file} ${_rc_content})
target_sources(${PROJECT_NAME} PRIVATE ${_rc_file})
