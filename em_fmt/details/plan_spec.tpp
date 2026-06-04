#pragma once
#include <cstddef>
#include <cstdint>
#include "format_spec.hpp"

namespace em {

/* 编译期归一化后的运行时 plan
 *
 * 把 format_spec 一次性解析成运行期 write_argument 直接使用的不变量。
 * 所有条件分支 (sign、prefix、align、base、zero_pad、is_arith) 都在编译期敲定，
 * 运行期 write_argument 只做 to_chars + fwrite 线性输出, 不再有任何 if 分支。
 *
 * 字段:
 *   sign_ch        : 0 表示无符号; '+' / '-' / ' ' 表示显式符号
 *   prefix         : "" / "0b" / "0B" / "0" / "0x" / "0X" (编译期字面量)
 *   prefix_len     : prefix 长度
 *   base           : to_chars 的 base, 2/8/10/16
 *   fill           : 0 表示无外层填充; 其它为填充字符
 *   align          : 0=无显式对齐(用默认), '<' / '>' / '^'
 *   width          : 外层 width (-1 = 不用)
 *   zero_pad       : 是否为 0-pad (即 {:#08x} 中 0 的含义)
 *   precision      : 浮点精度 (-1 = 用 default 6, hex 浮点用 0)
 *   chars_format   : 浮点的 chars_format
 *   is_hex_float   : a/A 类型 (需手动加 0x 前缀并大写化)
 *   is_general     : g/G/none 类型
 *   upper_float    : A/E/F/G 大写变体
 *   is_char        : c 类型 (整型族)
 *   is_string      : 字符串/布尔且 type==0/'s'
 *   is_bool        : 布尔 (字符串路径)
 *   is_ptr         : 指针 / nullptr
 *   ptr_upper      : 'P' 大写
 *   thousands      : n 千分位
 */
struct arg_plan {
    char            sign_ch{ 0 };
    char            prefix[3]{};
    unsigned char   prefix_len{ 0 };
    int             base{ 10 };
    char            fill{ 0 };
    char            align{ 0 };
    int             width{ -1 };
    bool            zero_pad{ false };
    int             precision{ -1 };
    std::chars_format chars_format{ std::chars_format::general };
    bool            is_hex_float{ false };
    bool            is_general{ false };
    bool            upper_float{ false };
    bool            is_char{ false };
    bool            is_string{ false };
    bool            is_bool{ false };
    bool            is_ptr{ false };
    bool            ptr_upper{ false };
    bool            thousands{ false };
};

/* 工具: 把 prefix 复制到 plan.prefix 里 (运行期用), 编译期用直接拼接更优
 * 这里因为 format_spec 来自 consteval runtime value, 我们在 consteval 里 copy. */
constexpr void set_prefix(arg_plan &p, const char *s) {
    p.prefix_len = 0;
    while (s[p.prefix_len] && p.prefix_len < 3) {
        p.prefix[p.prefix_len] = s[p.prefix_len];
        ++p.prefix_len;
    }
}

/* 编译期校验 + 归一化
 *
 * 把 spec 与目标 type_category 配对, 产生完整 plan.
 * 若组合非法, 不应调用此函数 (在 format_attributes 中已经 static_assert 过).
 *
 * 注: 用 constexpr (不是 consteval) 以便能在 fprint 的运行时上下文中调用.
 * 仍然只依赖 constexpr 输入 (format_spec 是字面量类型).
 */
constexpr arg_plan make_plan(const format_spec &s, int category) {
    arg_plan p{};
    p.fill        = s.fill;
    p.align       = s.align;
    p.width       = s.width;
    p.zero_pad    = s.zero_pad;
    p.precision   = s.precision;
    p.sign_ch     = s.sign;       // 默认是 '-'; spec 中给出 '+' / ' ' 时同步过来
    p.thousands   = s.thousands;

    auto cat = static_cast<arg_category>(category);
    bool is_arith = (cat == arg_category::integral) || (cat == arg_category::char_t) ||
                    (cat == arg_category::bool_t) || (cat == arg_category::floating) ||
                    (cat == arg_category::pointer) || (cat == arg_category::nullptr_t_v);

    // 1. 默认 align
    if (p.align == 0) {
        p.align = is_arith ? '>' : '<';
    }

    // 2. 字符串/布尔路径
    if (cat == arg_category::cstring || cat == arg_category::string || cat == arg_category::string_view) {
        p.is_string = true;
        return p;
    }
    if (cat == arg_category::bool_t) {
        p.is_bool = true;
        if (s.type == 0 || s.type == 's') {
            p.is_string = true; // 复用字符串路径 ("true"/"false")
        }
        // 否则按整型路径走 (b/B/d/o/x/X)
    }

    // 3. char 路径
    //    char 与字符串一致: 默认左对齐, 这是 std::format 行为.
    //    注意: 步骤 1 中 p.align 已被设置为 '>' (is_arith=true), 此处必须强制覆盖,
    //          不能用 "p.align == 0" 守卫 (那是错的——永远到不了).
    if (cat == arg_category::char_t && (s.type == 0 || s.type == 'c')) {
        p.is_char = true;
        p.align   = '<'; // 强制左对齐, 不论用户是否指定
        return p;
    }

    // 4. 指针 / nullptr
    if (cat == arg_category::pointer || cat == arg_category::nullptr_t_v) {
        p.is_ptr     = true;
        p.base       = 16;
        char type    = s.type == 0 ? 'p' : s.type;
        p.ptr_upper  = (type == 'P');
        if (type == 'p' || type == 'P') {
            set_prefix(p, p.ptr_upper ? "0X" : "0x");
        }
        return p;
    }

    // 5. 整型 (含 bool 当整数)
    if (cat == arg_category::integral || cat == arg_category::char_t ||
        (cat == arg_category::bool_t && !p.is_string)) {
        char type = s.type == 0 ? 'd' : s.type;
        if (type == 'c') {
            p.is_char = true;
            return p;
        }
        if (type == 'b') {
            p.base = 2;
            // prefix 仅在 alt-form 时设置; mag==0 时的特判放运行期
            // (oct 0 抑制 / bin 0 保留见 write_argument 普通路径)
            if (s.alt_form) {
                set_prefix(p, "0b");
            }
        } else if (type == 'B') {
            p.base = 2;
            if (s.alt_form) {
                set_prefix(p, "0B");
            }
        } else if (type == 'o') {
            p.base = 8;
            // '#' 且非零才加 '0'; 0 的特判在运行期抑制
            if (s.alt_form) {
                set_prefix(p, "0");
            }
        } else if (type == 'x') {
            p.base = 16;
            // BUG-02: 不带 alt-form 时不应输出 0x 前缀
            if (s.alt_form) {
                set_prefix(p, "0x");
            }
        } else if (type == 'X') {
            p.base = 16;
            p.ptr_upper = true;     // 大写化, 复用指针路径的字段
            // BUG-02: 不带 alt-form 时不应输出 0X 前缀
            if (s.alt_form) {
                set_prefix(p, "0X");
            }
        } else {
            p.base = 10;
            set_prefix(p, "");
        }
        return p;
    }

    // 6. 浮点
    if (cat == arg_category::floating) {
        char type = s.type == 0 ? 'g' : s.type;
        if (type == 'a') {
            p.is_hex_float = true;
            p.upper_float  = false;
            p.chars_format = std::chars_format::hex;
            // std::format 不在 hex 浮点前加 '0x' 前缀 (e.g. `{:a}` 1.5 → "1.8p+0")
            // (BUG-01 修复: 之前误加 0x, 现与 std::format 对齐)
            set_prefix(p, "");
        } else if (type == 'A') {
            p.is_hex_float = true;
            p.upper_float  = true;
            p.chars_format = std::chars_format::hex;
            set_prefix(p, "");
        } else if (type == 'e') {
            p.chars_format = std::chars_format::scientific;
            p.upper_float  = false;
        } else if (type == 'E') {
            p.chars_format = std::chars_format::scientific;
            p.upper_float  = true;
        } else if (type == 'f') {
            p.chars_format = std::chars_format::fixed;
            p.upper_float  = false;
        } else if (type == 'F') {
            p.chars_format = std::chars_format::fixed;
            p.upper_float  = true;
        } else if (type == 'g') {
            p.chars_format = std::chars_format::general;
            p.is_general   = true;
            p.upper_float  = false;
        } else { // 'G'
            p.chars_format = std::chars_format::general;
            p.is_general   = true;
            p.upper_float  = true;
        }
        return p;
    }

    return p;
}

} // namespace em
