cmake_minimum_required(VERSION 3.15)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 23)
enable_testing()

project(em_fmt_test)
add_subdirectory(em_fmt)

add_executable(speed_test test_speed.cpp)
target_link_libraries(speed_test PRIVATE 
    em_fmt 
    stdc++exp # 链接这个库以使用实验性功能(std::print)
    )
add_test(NAME Test_speed COMMAND speed_test)

add_executable(func_test test_func.cpp)
target_link_libraries(func_test PRIVATE em_fmt)
add_test(NAME Test_func COMMAND func_test)
