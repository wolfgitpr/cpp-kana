# Examples. Configure with -DCPP_KANA2_BUILD_EXAMPLES=ON.

add_executable(cpp-kana2-example-minimal minimal_usage.cpp)
target_link_libraries(cpp-kana2-example-minimal PRIVATE cpp-kana2::cpp-kana2 cpp-kana2_dev_flags)
