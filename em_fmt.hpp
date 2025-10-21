#pragma once
#include <array>
#include <stdio.h>
#include <charconv>
#include <algorithm>
namespace em {
template<size_t N> struct fixed_string {
    std::array<char, N> data_;
    unsigned int        need_arg_num     = 0;
    unsigned int        need_analyze_num = 0;
    bool                format_ok        = true;

    consteval bool check() {
        for (size_t i = 0; i < data_.size(); ++i) {
            if (data_[i] == '{') {
                if (data_[i + 1] == '{') {
                    ++need_analyze_num;
                    ++i; // skip next
                    continue;
                } else {
                    ++i;
                    for (size_t j = i; j < data_.size(); ++j, ++i) {
                        if (data_[j] == '}') {
                            ++need_arg_num;
                            ++need_analyze_num;
                            break;
                        }
                        /*如果到了最后一个字符还没找到}也报错*/
                        else if (j + 1 == data_.size()) {
                            format_ok = false;
                        }
                    }
                }
            } else if (data_[i] == '}') {
                if (data_[i + 1] == '}') {
                    ++need_analyze_num;
                    ++i; // skip next
                    continue;
                } else {
                    format_ok = false;
                }
            }
        }
        need_analyze_num == 0 ?: need_analyze_num++;
        return true;
    }

    consteval fixed_string(const char (&str)[N]) {
        std::copy_n(str, N, data_.begin());

        /*计算需要的参数数量*/
        check();
    }
};

struct arg_analyze_result {
    /*写n字节*/
    unsigned int write_bytes;
    /*跳过n字节*/
    unsigned int skip_bytes;
    /*是否有变量要输出*/
    bool have_arg;
    /*如果有并且是整数，进制是？*/
    unsigned int int_base;
    /*如果有并且是浮点数，格式是？*/
    std::chars_format float_format;
};

template<fixed_string S> struct StringLiteral {
    static constexpr std::array<unsigned int, S.need_arg_num> arg_attribute{};
    consteval StringLiteral() {
        if constexpr (S.need_arg_num == 0) {
        } else {
        }
    }
};

template<fixed_string format, typename... T> int fprint(FILE *stream, T... args) {
    using StrLit              = StringLiteral<format>;
    constexpr int acc_arg_num = sizeof...(args);
    static_assert(StrLit::arg_attribute.size() == acc_arg_num, "Error at args number");
    static_assert(format.format_ok, "Number of '{' or '}' Error");
    std::array<char, 8> buf;
    return 0;
}
    } // namespace em