cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 23)

add_library(em_fmt OBJECT)
target_sources(em_fmt PRIVATE em_fmt.hpp)

target_include_directories(em_fmt PUBLIC ${CMAKE_CURRENT_LIST_DIR})