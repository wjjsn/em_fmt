cmake_minimum_required(VERSION 3.15)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 23)
enable_testing()

project(em_fmt_test)
add_subdirectory(em_fmt)

add_compile_options(-O3)

add_executable(speed_test test_speed.cpp)
target_link_libraries(speed_test PRIVATE 
    em_fmt 
    stdc++exp # 链接这个库以使用实验性功能(std::print)
    )
add_test(NAME Test_speed COMMAND speed_test)

add_executable(func_test test_func.cpp)
target_link_libraries(func_test PRIVATE em_fmt)
add_test(NAME Test_func COMMAND func_test)

# 标准格式控制 (format spec) 运行时正确性测试
add_executable(spec_test test/test_spec.cpp)
target_link_libraries(spec_test PRIVATE em_fmt)
add_test(NAME Test_spec COMMAND spec_test)

# 栈预算测试: 编译一个 TU 实例化各种 fprint 调用,
# 用 -fstack-usage 跑静态分析, 解析 .su 文件, 校验
# 用户写的 write_argument / fprint 函数栈上字节数.
add_executable(stack_test test/test_stack.cpp)
target_link_libraries(stack_test PRIVATE em_fmt)

# 实例化 TU: 各种类型的 fprint 调用, 让编译器生成 em::write_argument 实例.
add_library(stack_test_inst OBJECT test/fprint_instantiations.cpp)
target_link_libraries(stack_test_inst PUBLIC em_fmt)
target_compile_options(stack_test_inst PRIVATE -fstack-usage)

# 把 .su 文件路径拼出来传给测试: .su 与 .o 在同一目录,
# 文件名是 <源文件名>.su
add_test(NAME Test_stack
    COMMAND stack_test
        ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/stack_test_inst.dir/test/fprint_instantiations.cpp.su)
