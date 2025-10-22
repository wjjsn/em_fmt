#pragma once

#include <array>
#include <cstdio>
#include <cstring>
#include "details/write_argument.tpp"
#include <tuple>
#include <type_traits>
#include <utility>

namespace em {

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

template <std::size_t N> struct format_attributes {
    std::array<arg_analyze_result, N> attributes{};
};

template <fixed_string format> consteval auto make_attributes() {
    format_attributes<format.need_analyze_num> result{};
    constexpr std::size_t len = format.data_.size() > 0 ? format.data_.size() - 1 : 0;
    std::size_t           idx = 0;
    std::size_t           i   = 0;

    while (i < len && idx < result.attributes.size()) {
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
            attr.write_bytes = static_cast<unsigned int>(i - literal_begin);
            result.attributes[idx++] = attr;
            continue;
        }

        if (i >= len) {
            break;
        }

        char ch = format.data_[i];
        if (ch == '{') {
            if (i + 1 < len && format.data_[i + 1] == '{') {
                attr.write_bytes = 1;
                attr.skip_bytes  = 1;
                result.attributes[idx++] = attr;
                i += 2;
                continue;
            }
            if (i + 1 < len && format.data_[i + 1] == '}') {
                attr.write_bytes = 0;
                attr.skip_bytes  = 2;
                attr.have_arg    = true;
                result.attributes[idx++] = attr;
                i += 2;
                continue;
            }
            ++i;
            continue;
        }

        if (ch == '}') {
            if (i + 1 < len && format.data_[i + 1] == '}') {
                attr.write_bytes = 1;
                attr.skip_bytes  = 1;
                result.attributes[idx++] = attr;
                i += 2;
                continue;
            }
            ++i;
            continue;
        }
    }

    return result;
}

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

template<fixed_string S> struct StringLiteral {
    inline static constexpr auto attributes = make_attributes<S>();
};

template <fixed_string format, typename... T>
int fprint(FILE *stream, T &&...args) {
    static_assert(format.format_ok, "Number of '{' or '}' Error");
    static_assert(format.need_arg_num == sizeof...(args), "Error at args number");

    const auto &attributes = StringLiteral<format>::attributes.attributes;
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
