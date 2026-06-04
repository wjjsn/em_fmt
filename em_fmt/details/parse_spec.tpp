#pragma once
#include <cstddef>
#include "format_spec.hpp"

namespace em {

/* 解析一个 {…} 替换字段中 '}' 之前的说明符字符串。
 *
 * 入参:
 *   begin – 指向 '{' 之后第一个字符
 *   end   – 指向匹配的 '}' 之后那个字符（或者 '}' 自身 —— 在调用方已知 end 位置的情况下）
 *
 * 出参:
 *   out_end – 设置为指向 '}' 处
 *
 * 返回:
 *   解析出的 format_spec; 调用方负责把 out_end 之前的所有源字符视为 skip_bytes。
 *
 * 语法 (与 std::format 一致, 简化为 v1 子集):
 *   format-spec   := [fill-and-align] [sign] ['#'] ['0'] [width] ['.' precision] ['L'] [type]
 *   fill-and-align := fill-char align | align
 *   fill-char      := 除 '{' '}' 之外的任意字符
 *   align          := '<' | '>' | '^'
 *   sign           := '+' | '-' | ' '
 *   width          := 正十进制整数
 *   precision      := 正十进制整数 (v1 不支持嵌套 {})
 *   type           := 单个字符
 */
consteval format_spec parse_spec(const char *begin, const char *end, const char *&out_end) {
    format_spec s{};
    const char *p = begin;

    // std::format 语法: '{' [arg-id] [':' format-spec] '}'.
    // 我们不支持 arg-id, 但 ':' 作为 spec 起始分隔符是 std::format 的写法,
    // 也是用户最自然的写法. 如果第一字符是 ':', 跳过它.
    if (p < end && p[0] == ':') {
        ++p;
    }

    // fill-and-align
    if (end - p >= 2) {
        char c0 = p[0];
        char c1 = p[1];
        if (c1 == '<' || c1 == '>' || c1 == '^') {
            // c0 是 fill, c1 是 align
            if (c0 == '{' || c0 == '}') {
                // 非法填充字符
                // 标记为 fail: 通过 type='?' 不会出现在合法集合中
                s.type = '?';
                out_end = p;
                return s;
            }
            s.fill = c0;
            s.align = c1;
            p += 2;
        } else if (c0 == '<' || c0 == '>' || c0 == '^') {
            s.align = c0;
            p += 1;
        }
    } else if (end - p == 1) {
        char c0 = p[0];
        if (c0 == '<' || c0 == '>' || c0 == '^') {
            s.align = c0;
            p += 1;
        }
    }

    // sign
    if (p < end && (p[0] == '+' || p[0] == '-' || p[0] == ' ')) {
        s.sign = p[0];
        ++p;
    }

    // '#'
    if (p < end && p[0] == '#') {
        s.alt_form = true;
        ++p;
    }

    // '0'
    // 检查 {0} 的非法情况：单独的 '0' 且后面没有其他内容
    if (p < end && p[0] == '0' && s.align == 0) {
        if (p + 1 == end) {
            // 只有 '0' 且没有后续内容 -> {0} 非法位置参数
            s.type = '?';
            out_end = p;
            return s;
        }
        s.zero_pad = true;
        ++p;
    }

    // width
    // BUG-05: 用 long long 累加, clamp 到 (1<<30) 防止整数溢出 UB.
    //         任意长数字串最终都被收敛到合法上限, 不再产生 UB.
    constexpr long long kMaxWidth = 1LL << 30;
    if (p < end && p[0] >= '0' && p[0] <= '9') {
        long long w = 0;
        while (p < end && p[0] >= '0' && p[0] <= '9') {
            w = w * 10 + (p[0] - '0');
            if (w > kMaxWidth) {
                w = kMaxWidth;
            }
            ++p;
        }
        s.width = static_cast<int>(w);
    }

    // precision
    if (p < end && p[0] == '.') {
        ++p;
        int pr = 0;
        bool any = false;
        while (p < end && p[0] >= '0' && p[0] <= '9') {
            pr = pr * 10 + (p[0] - '0');
            ++p;
            any = true;
        }
        // 检查 {:.} 的非法情况：'.' 后面没有数字
        if (!any) {
            s.type = '?';
            out_end = p;
            return s;
        }
        s.precision = pr;
    }

    // ',' (thousands separator for integers/floats; std::format syntax)
    if (p < end && p[0] == ',') {
        s.thousands = true;
        ++p;
    }

    // 'L'
    if (p < end && p[0] == 'L') {
        s.locale = true;
        ++p;
    }

    // type
    if (p < end) {
        s.type = p[0];
        ++p;
    }

    out_end = p;
    return s;
}

/* 在源字符串中定位匹配的 '}' (不把 '}}' 视为结束) 的逻辑
 * 内联在 format_attributes::make_attributes 中. */

} // namespace em
