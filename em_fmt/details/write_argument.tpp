#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <cstring>
#include <charconv>
#include "config.hpp"

#if USE_STATIC_BUFFER
static std::array<char, STATIC_BUFFER_SIZE> format_buffer;
#endif

namespace em {
struct arg_analyze_result {
    unsigned int      write_bytes{};
    unsigned int      skip_bytes{};
    bool              have_arg{};
    unsigned int      int_base{ 10 };
    std::chars_format float_format{ std::chars_format::general };
};

/*在这里特化模板或重载函数实现输出自定义格式*/

/*处理整型（包含 char, short, int, long...）*/
template<typename T>
    requires std::integral<T>
void write_argument(FILE *stream, const arg_analyze_result &attr, const T &value) {
    if constexpr (USE_STATIC_BUFFER) {
        auto [ptr, ec] =
            std::to_chars(format_buffer.data(), format_buffer.data() + format_buffer.size(), value, attr.int_base);
        if (ec == std::errc()) {
            fwrite(format_buffer.data(), sizeof(char), static_cast<std::size_t>(ptr - format_buffer.data()), stream);
        }
    } else if constexpr (USE_STACK_BUFFER) {
        std::array<std::uint8_t, STACK_BUFFER_SIZE> format_buffer;
        auto [ptr, ec] =
            std::to_chars(format_buffer.data(), format_buffer.data() + format_buffer.size(), value, attr.int_base);
        if (ec == std::errc()) {
            fwrite(format_buffer.data(), sizeof(char), static_cast<std::size_t>(ptr - format_buffer.data()), stream);
        }
    }
}
/*处理布尔型*/
void write_argument(FILE *stream, const arg_analyze_result &attr, const bool &value) {
    const char *str = value ? "true" : "false";
    fwrite(str, sizeof(char), std::strlen(str), stream);
}
/*处理字符串字面量等 char[N]*/
template<std::size_t N> void write_argument(FILE *stream, const arg_analyze_result &, const char (&value)[N]) {
    // 对字符串字面量等 char[N]，按 C-string 打印到 '\0'
    std::size_t length = std::strlen(value);
    fwrite(value, sizeof(char), length, stream);
}
/*处理浮点数*/
template<typename T>
    requires std::floating_point<T>
void write_argument(FILE *stream, const arg_analyze_result &attr, const T &value) {
    if constexpr (USE_STATIC_BUFFER) {
        if constexpr (FLOAT_FORMAT_BY_STD_TO_CHAR) {
            auto [ptr, ec] = std::to_chars(format_buffer.data(),
                                           format_buffer.data() + format_buffer.size(),
                                           value,
                                           attr.float_format);
            if (ec == std::errc()) {
                fwrite(format_buffer.data(),
                       sizeof(char),
                       static_cast<std::size_t>(ptr - format_buffer.data()),
                       stream);
            }
        } else if (FLOAT_FORMAT_BY_STD_SNPRINTF) {
            std::snprintf(format_buffer.data(), format_buffer.size(), "%f", static_cast<double>(value));
            fwrite(format_buffer.data(), sizeof(char), std::strlen(format_buffer.data()), stream);
        }

    } else if constexpr (USE_STACK_BUFFER) {
        std::array<char, STACK_BUFFER_SIZE> format_buffer;
        if constexpr (FLOAT_FORMAT_BY_STD_TO_CHAR) {
            auto [ptr, ec] = std::to_chars(format_buffer.data(),
                                           format_buffer.data() + format_buffer.size(),
                                           value,
                                           attr.float_format);
            if (ec == std::errc()) {
                fwrite(format_buffer.data(),
                       sizeof(char),
                       static_cast<std::size_t>(ptr - format_buffer.data()),
                       stream);
            }
        } else if (FLOAT_FORMAT_BY_STD_SNPRINTF) {
            std::snprintf(format_buffer.data(), format_buffer.size(), "%f", static_cast<double>(value));
            fwrite(format_buffer.data(), sizeof(char), std::strlen(format_buffer.data()), stream);
        }
    }
}
/*处理 std::string*/
void write_argument(FILE *stream, const arg_analyze_result &attr, const std::string &value) {
    fwrite(value.data(), sizeof(char), value.size(), stream);
}
/*处理 std::string_view*/
void write_argument(FILE *stream, const arg_analyze_result &attr, const std::string_view &value) {
    fwrite(value.data(), sizeof(char), value.size(), stream);
}

/*处理原始指针*/
template<typename T>
    requires std::is_pointer_v<T>
void write_argument(FILE *stream, const arg_analyze_result &attr, const T &value) {
    uintptr_t addr                                  = reinterpret_cast<uintptr_t>(value);
    if constexpr (USE_STATIC_BUFFER) {
        auto [ptr, ec] =
            std::to_chars(format_buffer.data(), format_buffer.data() + format_buffer.size(), addr, attr.int_base);
        if (ec == std::errc()) {
            fwrite(format_buffer.data(), sizeof(char), static_cast<std::size_t>(ptr - format_buffer.data()), stream);
        }
    } else if constexpr (USE_STACK_BUFFER) {
        std::array<char, STACK_BUFFER_SIZE> format_buffer;
        auto [ptr, ec] =
            std::to_chars(format_buffer.data(), format_buffer.data() + format_buffer.size(), addr, attr.int_base);
        if (ec == std::errc()) {
            fwrite(format_buffer.data(), sizeof(char), static_cast<std::size_t>(ptr - format_buffer.data()), stream);
        }
    }
}

void write_argument(FILE *stream, const arg_analyze_result &attr, const std::nullptr_t &value) {
    const char *str = "nullptr";
    fwrite(str, sizeof(char), std::strlen(str), stream);
}

template<typename T> void write_argument(FILE *stream, const arg_analyze_result &attr, const T &value) = delete;
}
