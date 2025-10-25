# em_fmt 0.1.0

`em_fmt` 是一个针对 C++23 的轻量级嵌入式友好格式化库实验，强调在编译期解析格式字符串并以极少依赖提供快速 `fprint` 输出接口。本次 0.1.0 版本主要聚焦于构建核心能力并提供示例和测试工程，便于继续迭代。

## 特性
- **编译期格式校验**：使用 `fixed_string` 在编译期解析格式字符串，提前检查花括号匹配与参数个数，让格式错误在编译阶段暴露。
- **嵌入式可用的轻量实现**：完全头文件形式、仅依赖 C++23 标准库，无额外第三方依赖，在资源受限的嵌入式或交叉编译场景中也能轻松集成。
- **广泛的参数支持**：当前支持整型、浮点、布尔、`std::string`、`std::string_view`、C 风格字符串以及原始指针和 `nullptr`。
- **可配置的缓冲策略**：通过 `details/config.hpp` 切换静态缓冲区或栈上缓冲区，并支持选择浮点格式化方案，便于按需权衡性能与内存占用。
- **极简构建依赖**：仅需 CMake 和支持 C++23 的编译器即可完成编译与测试，适合集成到现有构建流水线。

## 快速开始
1. 安装 C++23 编译器（如 GCC 13+ 或 Clang 16+）和 CMake 3.15 及以上版本。
2. 在仓库根目录执行：
   ```bash
   cmake -S . -B build
   cmake --build build
   ctest --test-dir build
   ```
   `func_test` 将演示格式化输出，`speed_test` 用于性能基准。

## 使用示例
```cpp
#include <em_fmt.hpp>

int main() {
    em::fprint<"answer={}, pi={}\n">(stdout, 42, 3.14);
    return 0;
}
```

## 配置项
可编辑 `em_fmt/details/config.hpp` 调整以下选项：
- `USE_STATIC_BUFFER` / `USE_STACK_BUFFER`：选择静态缓冲区或每次调用时的栈缓冲区。
- `STATIC_BUFFER_SIZE` / `STACK_BUFFER_SIZE`：控制缓冲区大小。
- `FLOAT_FORMAT_BY_STD_TO_CHAR` / `FLOAT_FORMAT_BY_STD_SNPRINTF`：选择浮点格式化方式。

## 后续计划
- 支持更多 STL 容器与自定义类型。
- 提供可扩展的格式说明符。
- 增加更多性能与正确性测试。

欢迎提交 Issue 或 PR 共同完善！
