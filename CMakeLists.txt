cmake_minimum_required(VERSION 3.15)
# set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 23)


set(PROJECT_NAME em_fmt)
project(${PROJECT_NAME})
add_executable(${PROJECT_NAME} 
    em_fmt.cpp
    )

# 链接这个库以使用实验性功能(std::print)
target_link_libraries(${PROJECT_NAME} PRIVATE
    -lstdc++exp)
# target_include_directories(${PROJECT_NAME} PRIVATE
#     "${CMAKE_CURRENT_LIST_DIR}")


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)