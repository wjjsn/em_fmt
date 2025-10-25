#pragma once

#include <array>
#include <cstdio>
#include <cstring>
#include "details/write_argument.tpp"
#include <tuple>
#include <type_traits>
#include <utility>

namespace em {
/*固定字符串，并进行基础解析*/
template <std::size_t N> struct fixed_string {
    std::array<char, N> data_{};
    unsigned int        need_arg_num     = 0;
    unsigned int        need_analyze_num = 0;
    bool                format_ok        = true;

    consteval void analyze() {
        constexpr std::size_t len = N == 0 ? 0 : N - 1;
        std::size_t           i   = 0;
        while (i < len && format_ok) {
            std::size_t literal_begin = i;
            while (i < len) {
                char ch = data_[i];
                if (ch == '{' || ch == '}') {
                    break;
                }
                ++i;
            }
            if (i > literal_begin) {
                ++need_analyze_num;
                continue;
            }

            if (i >= len) {
                break;
            }

            char ch = data_[i];
            if (ch == '{') {
                if (i + 1 >= len) {
                    format_ok = false;
                    break;
                }
                char next = data_[i + 1];
                if (next == '{') {
                    ++need_analyze_num;
                    i += 2;
                    continue;
                }
                if (next == '}') {
                    ++need_arg_num;
                    ++need_analyze_num;
                    i += 2;
                    continue;
                }
                format_ok = false;
                break;
            } else if (ch == '}') {
                if (i + 1 < len && data_[i + 1] == '}') {
                    ++need_analyze_num;
                    i += 2;
                    continue;
                }
                format_ok = false;
                break;
            }
        }
    }

    consteval fixed_string(const char (&str)[N]) : data_{} {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] = str[i];
        }
        analyze();
    }
};
/*根据基础解析进行详细解析，生成输出时属性*/
template<fixed_string format> struct format_attributes {
    consteval static auto make_attributes() {
        std::array<arg_analyze_result, format.need_analyze_num> attributes{};
        constexpr std::size_t len = format.data_.size() > 0 ? format.data_.size() - 1 : 0;
        std::size_t           idx = 0;
        std::size_t           i   = 0;

        while (i < len && idx < attributes.size()) {
            arg_analyze_result attr{};
            std::size_t        literal_begin = i;
            while (i < len) {
                char ch = format.data_[i];
                if (ch == '{' || ch == '}') {
                    break;
                }
                ++i;
            }

            if (i > literal_begin) {
                attr.write_bytes  = static_cast<unsigned int>(i - literal_begin);
                attributes[idx++] = attr;
                continue;
            }

            if (i >= len) {
                break;
            }

            char ch = format.data_[i];
            if (ch == '{') {
                if (i + 1 < len && format.data_[i + 1] == '{') {
                    attr.write_bytes  = 1;
                    attr.skip_bytes   = 1;
                    attributes[idx++] = attr;
                    i += 2;
                    continue;
                }
                if (i + 1 < len && format.data_[i + 1] == '}') {
                    attr.write_bytes  = 0;
                    attr.skip_bytes   = 2;
                    attr.have_arg     = true;
                    attributes[idx++] = attr;
                    i += 2;
                    continue;
                }
                ++i;
                continue;
            }

            if (ch == '}') {
                if (i + 1 < len && format.data_[i + 1] == '}') {
                    attr.write_bytes  = 1;
                    attr.skip_bytes   = 1;
                    attributes[idx++] = attr;
                    i += 2;
                    continue;
                }
                ++i;
                continue;
            }
        }

        return attributes;
    }

    constexpr static std::array<arg_analyze_result, format.need_analyze_num> attributes = make_attributes();
};

template <typename Tuple, typename Func, std::size_t... I>
void dispatch_arg_impl(Tuple &&tup, std::size_t index, Func &&func, std::index_sequence<I...>) {
    [[maybe_unused]] bool handled = ((index == I ? (func(std::get<I>(tup)), true) : false) || ...);
    (void)handled;
}

template <typename Tuple, typename Func>
void dispatch_arg(Tuple &&tup, std::size_t index, Func &&func) {
    dispatch_arg_impl(
        std::forward<Tuple>(tup),
        index,
        std::forward<Func>(func),
        std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<Tuple>>>{});
}

template <fixed_string format, typename... T>
int fprint(FILE *stream, T &&...args) {
    static_assert(format.format_ok, "Number of '{' or '}' Error");
    static_assert(format.need_arg_num == sizeof...(args), "Error at args number");

    const auto &attributes = format_attributes<format>::attributes;
    const char *cursor     = format.data_.data();
    auto        arg_tuple  = std::forward_as_tuple(std::forward<T>(args)...);
    std::size_t arg_index  = 0;

    for (const auto &attr : attributes) {
        if (attr.write_bytes > 0) {
            fwrite(cursor, sizeof(char), attr.write_bytes, stream);
        }
        cursor += attr.write_bytes;

        if (attr.have_arg) {
            dispatch_arg(arg_tuple, arg_index, [&](const auto &value) {
                write_argument(stream, attr, value);
            });
            ++arg_index;
        }

        cursor += attr.skip_bytes;
    }

    return 0;
}

} // namespace em
