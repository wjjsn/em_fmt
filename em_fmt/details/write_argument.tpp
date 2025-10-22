#pragma once
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <cstring>
#include <charconv>

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
    char buffer[std::numeric_limits<T>::digits10 + 3]{};
    auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value, attr.int_base);
    if (ec == std::errc()) {
        fwrite(buffer, sizeof(char), static_cast<std::size_t>(ptr - buffer), stream);
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
    char buffer[128]{}; //TDDO：应该考虑这个内存的申请方式
    auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value, attr.float_format);
    if (ec == std::errc()) {
        fwrite(buffer, sizeof(char), static_cast<std::size_t>(ptr - buffer), stream);
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
    char      buffer[2 + sizeof(uintptr_t) * 2 + 1] = {}; // TODO
    auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), addr, attr.int_base);
    if (ec == std::errc()) {
        fwrite(buffer, sizeof(char), static_cast<std::size_t>(ptr - buffer), stream);
    }
}

void write_argument(FILE *stream, const arg_analyze_result &attr, const std::nullptr_t &value) {
    const char *str = "nullptr";
    fwrite(str, sizeof(char), std::strlen(str), stream);
}

template<typename T> void write_argument(FILE *stream, const arg_analyze_result &attr, const T &value) = delete;

}
