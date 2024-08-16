project(Test)

file(GLOB_RECURSE _src *.h *.cpp)

if (WIN32)
    set(PLATFORM_LIB dwmapi.lib)
endif ()

add_executable(${PROJECT_NAME} ${_src})

target_include_directories(${PROJECT_NAME} PUBLIC .)

target_link_libraries(${PROJECT_NAME} PUBLIC
        ${PLATFORM_LIB} cpp-kana
)