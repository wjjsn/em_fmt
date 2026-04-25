# em_fmt 0.1.0

`em_fmt` is an experimental, lightweight, and embedded-friendly formatting library for C++23. It performs compile-time parsing of format strings and keeps dependencies to a bare minimum while offering a fast `fprint` helper at runtime. Version 0.1.0 lays down the core infrastructure together with sample targets so future iterations can focus on feature growth.

## Highlights
- **Compile-time validation** – `fixed_string` analyses the format literal before compilation finishes, ensuring braces are balanced and argument counts match so mistakes surface during the build.
- **Embedded-ready footprint** – header-only, relying solely on the C++23 standard library with no third-party dependencies, making it easy to drop into constrained or cross-compilation environments.
- **Wide argument coverage** – integers, floating-point numbers, booleans, `std::string`, `std::string_view`, C-style strings, raw pointers, and `nullptr` are already supported.
- **Configurable buffering** – tweak `details/config.hpp` to switch between static or stack-based buffers and to choose the floating-point formatting backend, helping balance performance and memory use.
- **Minimal build requirements** – only CMake and a C++23-capable compiler are needed to build and test, easing integration into existing pipelines.

## Getting started
1. Install a C++23 capable toolchain (GCC 13+, Clang 16+, or MSVC with /std:c++latest) and CMake ≥ 3.15.
2. Build and run the bundled tests:
   ```bash
   cmake -S . -B build
   cmake --build build
   ctest --test-dir build
   ```
   `func_test` demonstrates formatted output while `speed_test` is designed for benchmarking.

## Usage example
```cpp
#include <em_fmt.hpp>

int main() {
    em::fprint<"answer={}, pi={}\n">(stdout, 42, 3.14);
    return 0;
}
```

## Configuration knobs
Edit `em_fmt/details/config.hpp` to adjust:
- `USE_STATIC_BUFFER` vs `USE_STACK_BUFFER` for buffer lifetime management.
- `STATIC_BUFFER_SIZE` and `STACK_BUFFER_SIZE` for buffer sizing.
- `FLOAT_FORMAT_BY_STD_TO_CHAR` vs `FLOAT_FORMAT_BY_STD_SNPRINTF` for floating-point formatting.

## Roadmap
- Extend support to additional STL containers and user-defined types.
- Provide richer, customizable format specifiers.
- Add more benchmarks and correctness suites.

Contributions and feedback are very welcome!
