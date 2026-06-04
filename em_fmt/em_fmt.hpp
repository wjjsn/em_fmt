#pragma once

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <utility>

#include "details/format_spec.hpp"
#include "details/parse_spec.tpp"
#include "details/plan_spec.tpp"
#include "details/write_argument.tpp"

namespace em {

/* 固定字符串: 仅作为编译期格式串的容器, 解析在 format_attributes 中完成。*/
template <std::size_t N> struct fixed_string {
    std::array<char, N> data_{};

    consteval fixed_string(const char (&str)[N]) : data_{} {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] = str[i];
        }
    }
};

/* 每段 (literal / {{ / }} / {…}) 的编译期属性.
 *
 * 所有内容在 consteval 阶段算出, 运行期只读不写。
 */
struct arg_analyze_result {
    unsigned int write_bytes{};   // 字面量输出字节数
    unsigned int skip_bytes{};    // 源串中向前推进的字节数 (write_bytes 之后)
    bool         have_arg{};      // 此段是否关联运行时参数
    format_spec  spec{};          // 解析后的 spec (have_arg 时有意义)
};

/* 把源格式串展开成段表 (一次 consteval 扫描)
 *
 * 同时完成:
 *   - 花括号配平检查
 *   - 转义 `{{` / `}}` 的字面化
 *   - {:spec} 中 spec 的 parse
 *   - arg_id (`{0}` / `{name}`) 的拒绝
 */
template <fixed_string format> struct format_attributes {
  private:
    consteval static std::size_t scan_segment_count() {
        constexpr std::size_t len = format.data_.size() > 0 ? format.data_.size() - 1 : 0;
        std::size_t           i   = 0;
        std::size_t           cnt = 0;
        while (i < len) {
            char ch = format.data_[i];
            if (ch == '{') {
                if (i + 1 < len && format.data_[i + 1] == '{') {
                    ++cnt;
                    i += 2;
                } else {
                    ++cnt;
                    std::size_t j = i + 1;
                    while (j < len && format.data_[j] != '}') {
                        ++j;
                    }
                    // i = j+1: spec 段已经把闭合 '}' 算进 skip_bytes,
                    // 下一轮迭代从闭合 '}' 之后开始, 此时若遇到 '}}' 则是
                    // 在 spec 闭合之后跟随的 '}}' 转义, 写一个 '}'.
                    i = (j < len) ? j + 1 : len;
                }
            } else if (ch == '}') {
                ++cnt;
                i += (i + 1 < len && format.data_[i + 1] == '}') ? 2 : 1;
            } else {
                std::size_t begin = i;
                while (i < len && format.data_[i] != '{' && format.data_[i] != '}') {
                    ++i;
                }
                ++cnt;
                (void)begin;
            }
        }
        return cnt;
    }

  public:
    consteval static auto make_attributes() {
        constexpr std::size_t len = format.data_.size() > 0 ? format.data_.size() - 1 : 0;
        constexpr std::size_t cap = scan_segment_count();
        std::array<arg_analyze_result, cap> tmp{};
        std::size_t                          idx       = 0;
        unsigned int                         arg_count = 0;
        std::size_t                          i         = 0;

        while (i < len && idx < cap) {
            char ch = format.data_[i];

            if (ch != '{' && ch != '}') {
                std::size_t begin = i;
                while (i < len && format.data_[i] != '{' && format.data_[i] != '}') {
                    ++i;
                }
                tmp[idx].write_bytes = static_cast<unsigned int>(i - begin);
                tmp[idx].skip_bytes  = 0;
                ++idx;
                continue;
            }

            if (ch == '{') {
                if (i + 1 < len && format.data_[i + 1] == '{') {
                    tmp[idx].write_bytes = 1;
                    tmp[idx].skip_bytes  = 1;
                    ++idx;
                    i += 2;
                    continue;
                }
                std::size_t j = i + 1;
                // 第一个 '}' 即为 spec 闭合. spec body 中不应有 '{' 或 '}'.
                while (j < len && format.data_[j] != '}') {
                    ++j;
                }
                const char *fmt_begin  = format.data_.data();
                const char *spec_begin = fmt_begin + i + 1;
                const char *spec_end   = (j < len) ? (fmt_begin + j) : (fmt_begin + len);
                const char *parsed_end = nullptr;
                format_spec spec       = parse_spec(spec_begin, spec_end, parsed_end);
                tmp[idx].write_bytes   = 0;
                // skip_bytes 覆盖 '{' + spec body + 闭合 '}', 即从 i 到 j 全部.
                tmp[idx].skip_bytes    = static_cast<unsigned int>((j < len ? j : len) - i) + 1;
                tmp[idx].have_arg      = true;
                tmp[idx].spec          = spec;
                ++arg_count;
                ++idx;
                // i = j+1: 下一轮从闭合 '}' 之后开始, 那里若有 '}}' 则是
                // 跟在 spec 后的 '}}' 转义, 由转义分支处理.
                i = (j < len) ? j + 1 : len;
                continue;
            }

            // ch == '}'
            if (i + 1 < len && format.data_[i + 1] == '}') {
                tmp[idx].write_bytes = 1;
                tmp[idx].skip_bytes  = 1;
                ++idx;
                i += 2;
            } else {
                tmp[idx].write_bytes = 1;
                tmp[idx].skip_bytes  = 0;
                ++idx;
                i += 1;
            }
        }

        return std::pair{ tmp, arg_count };
    }

    constexpr static auto                                       build        = make_attributes();
    constexpr static std::array<arg_analyze_result, build.first.size()> attributes = build.first;
    constexpr static unsigned int                                need_arg_num = build.second;

    /* 把"第 i 个 arg"映射到"第 j 个 attribute (have_arg == true 的那个)"
     *
     * 编译期计算, 用于在 fprint 实例化时按 arg 索引查找正确的 spec.
     * 形如 ["x " (literal), "y" (literal), "{:d}" (arg), "z" (literal), "{:s}" (arg)]
     * 对应 arg_attr_indices = [2, 4].
     */
    consteval static auto make_arg_attr_indices() {
        std::array<unsigned int, need_arg_num> out{};
        unsigned int a_idx = 0;
        for (std::size_t i = 0; i < attributes.size(); ++i) {
            if (attributes[i].have_arg) {
                out[a_idx++] = static_cast<unsigned int>(i);
            }
        }
        return out;
    }
    constexpr static std::array<unsigned int, need_arg_num> arg_attr_indices = make_arg_attr_indices();
};

/* 编译期校验: 第 I 个 spec 对第 I 个 arg 类型是否合法
 *
 * 这里 spec 的查找必须用 format_attributes<format>::arg_attr_indices[I],
 * 因为 attributes 中夹杂了字面量段.
 */
template <fixed_string fmt, std::size_t I, typename T>
consteval const arg_analyze_result &spec_for_arg() {
    return em::format_attributes<fmt>::attributes[
        em::format_attributes<fmt>::arg_attr_indices[I]];
}

template <fixed_string fmt, std::size_t I, typename T>
consteval bool check_one_arg() {
    return em::is_valid_spec_for_type(spec_for_arg<fmt, I, T>().spec,
                                      em::category_of<T>());
}

template <fixed_string fmt, typename ArgTuple, std::size_t... I>
consteval bool check_all_args_impl(std::index_sequence<I...>) {
    return (check_one_arg<fmt, I, std::tuple_element_t<I, ArgTuple>>() && ...);
}

template <fixed_string fmt, typename ArgTuple>
consteval bool check_all_args() {
    return check_all_args_impl<fmt, ArgTuple>(
        std::make_index_sequence<std::tuple_size_v<ArgTuple>>{});
}

/* 编译期归一化: 把每段的 spec 与其对应 arg 类别组成 arg_plan.
 *
 * 输入是 format_attributes<format>::attributes (constexpr) 与
 * std::tuple<T...> (模板参数), 全是常量; 输出是 consteval 的
 * std::array<arg_plan, sizeof...(args)>. 这一次常量求值发生在
 * fprint 实例化时, 结果被绑定到函数级 static constexpr, 运行期
 * 不会再做任何"算 plan"的工作.
 */
template <fixed_string fmt, std::size_t I, typename T>
struct arg_plan_value {
    constexpr static arg_plan value =
        make_plan(spec_for_arg<fmt, I, T>().spec, em::category_of<T>());
};

template <fixed_string format, typename Tuple, std::size_t... I>
consteval auto compute_arg_plans_impl(std::index_sequence<I...>) {
    return std::array<arg_plan, sizeof...(I)>{
        arg_plan_value<format, I, std::tuple_element_t<I, Tuple>>::value...
    };
}

template <fixed_string format, typename Tuple>
consteval auto compute_arg_plans() {
    return compute_arg_plans_impl<format, Tuple>(
        std::make_index_sequence<std::tuple_size_v<Tuple>>{});
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

template <fixed_string format, typename... T>
int fprint(FILE *stream, T &&...args) {
    static_assert(format_attributes<format>::need_arg_num == sizeof...(args),
                  "em_fmt: argument count mismatch");

    using ArgTuple = std::tuple<T...>;
    static_assert(check_all_args<format, ArgTuple>(),
                  "em_fmt: one or more format spec is invalid for its argument type");

    /* 编译期归一化: 在 fprint 实例化时, 每段 spec 配上对应 arg 类型, 算出
     * 完整 arg_plan. 数组绑定到 static constexpr, 编译器把它放进 .rodata,
     * 函数栈上不占空间, 运行期只读. */
    static constexpr std::array<arg_plan, sizeof...(args)> arg_plans =
        compute_arg_plans<format, ArgTuple>();

    const char *cursor    = format.data_.data();
    auto        arg_tuple = std::forward_as_tuple(std::forward<T>(args)...);
    std::size_t arg_index = 0;

    for (const auto &attr : format_attributes<format>::attributes) {
        if (attr.write_bytes > 0) {
            fwrite(cursor, sizeof(char), attr.write_bytes, stream);
        }
        cursor += attr.write_bytes;

        if (attr.have_arg) {
            const arg_plan &p = arg_plans[arg_index];
            dispatch_arg(arg_tuple, arg_index, [&](const auto &value) {
                write_argument(stream, p, value);
            });
            ++arg_index;
        }

        cursor += attr.skip_bytes;
    }

    return 0;
}

} // namespace em
