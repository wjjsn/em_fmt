#pragma once
#include <charconv>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>

namespace em {

/* 编译期格式说明符 (POD)
 *
 * 由 details/parse_spec.tpp 中的 parse_spec() 在常量求值期间填充，
 * 由 details/write_argument.tpp 中的 write_argument() 在运行时使用。
 *
 * 默认值对应 "空说明符" {} —— 即 std::format 中的默认呈现。
 */
struct format_spec {
    char        fill{ ' ' };      // 单字符填充，默认为空格
    char        align{ 0 };       // 0=未指定（按 type 决定默认），1='<'，2='>'，3='^'
    char        sign{ '\0' };     // '+'、'-'、' '；'\0' = 未指定 (BUG-07)
    bool        alt_form{ false };// '#'
    bool        zero_pad{ false };// '0'
    bool        locale{ false };  // 'L'  (v1 拒绝)
    bool        thousands{ false };// ',' (仅整型, v1)
    char        type{ 0 };        // 单字符类型，0=未指定
    int         width{ -1 };      // -1=未指定
    int         precision{ -1 };  // -1=未指定
};

/* 参数类型分类 (运行时 + 编译期) */
enum class arg_category : int {
    integral       = 1,
    char_t         = 2,
    bool_t         = 3,
    floating       = 4,
    cstring        = 5,
    string         = 6,
    string_view    = 7,
    pointer        = 8,
    nullptr_t_v    = 9,
    unknown        = 0
};

template <typename T>
constexpr int category_of() {
    using U = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<U, bool>) {
        return static_cast<int>(arg_category::bool_t);
    } else if constexpr (std::is_same_v<U, std::nullptr_t>) {
        return static_cast<int>(arg_category::nullptr_t_v);
    } else if constexpr (std::is_pointer_v<U>) {
        return static_cast<int>(arg_category::pointer);
    } else if constexpr (std::is_floating_point_v<U>) {
        return static_cast<int>(arg_category::floating);
    } else if constexpr (std::is_same_v<U, std::string>) {
        return static_cast<int>(arg_category::string);
    } else if constexpr (std::is_same_v<U, std::string_view>) {
        return static_cast<int>(arg_category::string_view);
    } else if constexpr (std::is_same_v<U, char>) {
        return static_cast<int>(arg_category::char_t);
    } else if constexpr (std::is_array_v<U> && std::is_same_v<std::remove_extent_t<U>, char>) {
        return static_cast<int>(arg_category::cstring);
    } else if constexpr (std::integral<U>) {
        return static_cast<int>(arg_category::integral);
    } else {
        return static_cast<int>(arg_category::unknown);
    }
}

/* 检查说明符字符是否在某个允许集合中 */
constexpr bool spec_type_in(format_spec s, std::initializer_list<char> allowed) {
    if (s.type == 0) {
        return true; // empty type always allowed
    }
    for (char c : allowed) {
        if (s.type == c) {
            return true;
        }
    }
    return false;
}

/* 编译期校验：说明符是否对该参数类型合法
 *
 * 使用 constexpr 而非 consteval, 以便在 static_assert 中与来自
 * constexpr 数组的 spec 元素一起使用。
 */
constexpr bool is_valid_spec_for_type(const format_spec &s, int category) {
    using c = arg_category;
    auto cat = static_cast<c>(category);

    // 1. 精度
    if (s.precision >= 0) {
        bool ok = (cat == c::floating) || (cat == c::cstring) || (cat == c::string) || (cat == c::string_view);
        if (!ok) {
            return false;
        }
    }

    // 2. sign / '#' / '0' / L 仅算术
    bool is_arith = (cat == c::integral) || (cat == c::char_t) || (cat == c::bool_t) ||
                    (cat == c::floating)  || (cat == c::pointer) || (cat == c::nullptr_t_v);
    if (!is_arith) {
        // 非算术类型只接受 "未指定 sign" 状态; '\0' (BUG-07) 与 '-' (旧默认) 等价
        if (s.sign != '\0' || s.alt_form || s.zero_pad || s.locale) {
            return false;
        }
    }

    // 3. L flag (v1 拒绝)
    if (s.locale) {
        return false;
    }

    // 4. ',' thousands 仅整型 (BUG-08):
    //   std::format 在 GCC 15 也不接受 `:,f`; TODO 中的 (a) 选项实际上
    //   不是标准行为, (b) 才是与 std::format 对齐的正确选项. 保持拒绝.
    if (s.thousands) {
        bool ok = (cat == c::integral) || (cat == c::char_t) || (cat == c::bool_t);
        if (!ok) {
            return false;
        }
    }

    // 4. type 字符白名单
    switch (cat) {
    case c::integral:
        return spec_type_in(s, {'b', 'B', 'c', 'd', 'o', 'x', 'X'});
    case c::char_t:
        return spec_type_in(s, {'b', 'B', 'c', 'd', 'o', 'x', 'X'});
    case c::bool_t:
        // 'c' 不允许出现在 bool 上
        return spec_type_in(s, {'s', 'b', 'B', 'd', 'o', 'x', 'X'});
    case c::floating:
        return spec_type_in(s, {'a', 'A', 'e', 'E', 'f', 'F', 'g', 'G'});
    case c::cstring:
    case c::string:
    case c::string_view:
        return spec_type_in(s, {'s'});
    case c::pointer:
    case c::nullptr_t_v:
        return spec_type_in(s, {'p', 'P'});
    case c::unknown:
    default:
        return false;
    }
}

} // namespace em
