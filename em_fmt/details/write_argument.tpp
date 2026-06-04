#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <charconv>
#include <type_traits>

#include "config.hpp"
#include "format_spec.hpp"
#include "plan_spec.tpp"

namespace em {

/* 写一个 fill 字符 N 次.
 * 使用 8 字节栈 buf, N < 8 时单次 fwrite, 否则循环. */
inline void write_n_fill(FILE *stream, char ch, std::size_t count) {
    if (count == 0) {
        return;
    }
    char buf[8];
    for (std::size_t i = 0; i < 8; ++i) {
        buf[i] = ch;
    }
    while (count >= 8) {
        fwrite(buf, 1, 8, stream);
        count -= 8;
    }
    if (count) {
        fwrite(buf, 1, count, stream);
    }
}

/*==================================================================*/
/*  整型 (含 char、bool-当整数, 不含 bool-string 路径)                 */
/*==================================================================*/

/* 千分位输出. noinline, 自身用 32 字节栈, 不与整型普通路径的 digits[24] 叠加. */
__attribute__((noinline))
static void write_thousands_u64(FILE *stream, const arg_plan &p,
                                std::uint64_t mag, char sign_ch) {
    // digits_buf 独立缓冲, 避免与 body 的拼装区覆盖
    // 20 位 uint64 + null 足够; 24 字节
    char digits_buf[24];
    auto [qd, ecd] =
        std::to_chars(digits_buf, digits_buf + sizeof(digits_buf), mag, 10);
    if (ecd != std::errc()) {
        write_n_fill(stream, p.fill ? p.fill : ' ',
                     p.width > 0 ? static_cast<std::size_t>(p.width) : 0);
        return;
    }
    std::size_t dlen = static_cast<std::size_t>(qd - digits_buf);

    // body 组装: [sign][digits with ',' 3-1-...][prefix 0]
    // 20 数字 + 7 个逗号 + 1 符号 + 1 prefix = 29 → 32 字节足够
    char body[32];
    std::size_t blen = 0;
    if (sign_ch) {
        body[blen++] = sign_ch;
    }
    std::size_t first_group = dlen % 3;
    if (first_group == 0 && dlen > 0) {
        first_group = 3;
    }
    std::size_t copied = 0;
    for (std::size_t i = 0; i < dlen; ++i) {
        if (copied == first_group && i != 0) {
            body[blen++] = ',';
            first_group += 3;
        }
        body[blen++] = digits_buf[i];
        ++copied;
    }
    // prefix (n 没有 prefix; 在 make_plan 里 set_prefix("") 不写)
    for (unsigned k = 0; k < p.prefix_len; ++k) {
        body[blen++] = p.prefix[k];
    }

    // BUG-06: zero-pad (sign-aware) 路径. 整型 zero-pad 与浮点对齐相同:
    //         sign 单独写, 然后 '0' 填到 width, 最后写 digits+','.
    if (p.zero_pad && p.width > 0 && static_cast<int>(blen) < p.width) {
        std::size_t pad = static_cast<std::size_t>(p.width) - blen;
        if (sign_ch) {
            fputc(sign_ch, stream);
        }
        write_n_fill(stream, '0', pad);
        fwrite(body + (sign_ch ? 1 : 0), 1, blen - (sign_ch ? 1 : 0), stream);
        return;
    }

    if (p.width > 0 && static_cast<int>(blen) < p.width) {
        std::size_t pad     = static_cast<std::size_t>(p.width) - blen;
        char        fill_ch = p.fill ? p.fill : ' ';
        if (p.align == '>') {
            write_n_fill(stream, fill_ch, pad);
        } else if (p.align == '^') {
            write_n_fill(stream, fill_ch, pad / 2);
        }
        fwrite(body, 1, blen, stream);
        if (p.align == '<') {
            write_n_fill(stream, fill_ch, pad);
        } else if (p.align == '^') {
            write_n_fill(stream, fill_ch, pad - pad / 2);
        }
    } else {
        fwrite(body, 1, blen, stream);
    }
}

template <typename T>
    requires std::integral<T> && (!std::is_same_v<T, bool>) && (!std::is_same_v<T, char>)
inline void write_argument(FILE *stream, const arg_plan &p, const T &value) {
    using U = std::make_unsigned_t<T>;

    // 1. magnitude 与符号
    bool negative = false;
    U    mag      = 0;
    if constexpr (std::is_signed_v<T>) {
        if (value < 0) {
            negative = true;
            mag = static_cast<U>(-(value + 1)) + 1;
        } else {
            mag = static_cast<U>(value);
        }
    } else {
        mag = static_cast<U>(value);
    }

    // 2. 决定 sign_ch
    char sign_ch = 0;
    if (negative) {
        sign_ch = '-';
    } else if (p.sign_ch == '+') {
        sign_ch = '+';
    } else if (p.sign_ch == ' ') {
        sign_ch = ' ';
    }

    // 3. 千分位: 独立路径 (提取到 noinline 助手, 避免大栈 buffer 拉高本函数)
    if (p.thousands) {
        write_thousands_u64(stream, p, static_cast<std::uint64_t>(mag), sign_ch);
        return;
    }

    // 4. 普通路径
    // mag=0 时的 prefix 抑制 (BUG-03 修复):
    //   - oct: "0" 与 "00" 等价, std::format 抑制 '0'
    //   - bin: "0b0" 是合法的 0 表示, 必须保留 (NEW)
    //   - hex/x: 前缀仅在 alt-form 时由 make_plan 设置,
    //           此处 mag=0 仍按是否 set_prefix 决定
    bool show_prefix = p.prefix_len > 0;
    if (mag == 0 && p.base == 8) {
        show_prefix = false;
    }

    // 'c' 类型: 输出单个字符
    if (p.is_char) {
        char ch = static_cast<char>(value);
        std::size_t pad     = (p.width > 0) ? static_cast<std::size_t>(p.width) - 1 : 0;
        char        fill_ch = p.fill ? p.fill : ' ';
        char        al      = p.align ? p.align : '>';
        if (pad > 0) {
            if (al == '>') {
                write_n_fill(stream, fill_ch, pad);
            } else if (al == '^') {
                write_n_fill(stream, fill_ch, pad / 2);
            }
            fputc(ch, stream);
            if (al == '<') {
                write_n_fill(stream, fill_ch, pad);
            } else if (al == '^') {
                write_n_fill(stream, fill_ch, pad - pad / 2);
            }
        } else {
            fputc(ch, stream);
        }
        return;
    }

    // 65 字节栈 buf: 足够 uint64 binary (64 chars) + 余裕
    char digits[65];
    auto [q, ec] =
        std::to_chars(digits, digits + sizeof(digits), static_cast<std::uint64_t>(mag), p.base);
    if (ec != std::errc()) {
        if (p.width > 0) {
            write_n_fill(stream, p.fill ? p.fill : ' ',
                         static_cast<std::size_t>(p.width));
        }
        return;
    }
    std::size_t dlen = static_cast<std::size_t>(q - digits);

    // 'X' 大写化: 在 digits 缓冲上原地改写, 然后整体写出去
    if (p.base == 16 && p.ptr_upper) {
        for (std::size_t i = 0; i < dlen; ++i) {
            char c = digits[i];
            if (c >= 'a' && c <= 'f') {
                digits[i] = static_cast<char>(c - 'a' + 'A');
            }
        }
    }

    // sign 长度 + prefix 长度
    unsigned sign_prefix_len = 0;
    if (sign_ch) {
        ++sign_prefix_len;
    }
    if (show_prefix) {
        sign_prefix_len += p.prefix_len;
    }
    std::size_t digit_count = dlen;
    std::size_t blen        = sign_prefix_len + digit_count;

    // 5. zero_pad (sign-aware)
    if (p.zero_pad && p.width > 0 && static_cast<int>(blen) < p.width) {
        std::size_t pad = static_cast<std::size_t>(p.width) - blen;
        if (sign_ch) {
            fputc(sign_ch, stream);
        }
        if (show_prefix) {
            fwrite(p.prefix, 1, p.prefix_len, stream);
        }
        write_n_fill(stream, '0', pad);
        fwrite(digits, 1, dlen, stream);
        return;
    }

    // 6. 普通 width / fill / align
    if (p.width > 0 && static_cast<int>(blen) < p.width) {
        std::size_t pad     = static_cast<std::size_t>(p.width) - blen;
        char        fill_ch = p.fill ? p.fill : ' ';
        if (p.align == '>') {
            write_n_fill(stream, fill_ch, pad);
        } else if (p.align == '^') {
            write_n_fill(stream, fill_ch, pad / 2);
        }
        if (sign_ch) {
            fputc(sign_ch, stream);
        }
        if (show_prefix) {
            fwrite(p.prefix, 1, p.prefix_len, stream);
        }
        fwrite(digits, 1, dlen, stream);
        if (p.align == '<') {
            write_n_fill(stream, fill_ch, pad);
        } else if (p.align == '^') {
            write_n_fill(stream, fill_ch, pad - pad / 2);
        }
    } else {
        if (sign_ch) {
            fputc(sign_ch, stream);
        }
        if (show_prefix) {
            fwrite(p.prefix, 1, p.prefix_len, stream);
        }
        fwrite(digits, 1, dlen, stream);
    }
}

/*==================================================================*/
/*  char                                                              */
/*==================================================================*/

template <typename T>
    requires std::same_as<T, char>
inline void write_argument(FILE *stream, const arg_plan &p, const T &value) {
    if (p.is_char) {
        std::size_t pad     = (p.width > 0) ? static_cast<std::size_t>(p.width) - 1 : 0;
        char        fill_ch = p.fill ? p.fill : ' ';
        if (pad > 0) {
            if (p.align == '>') {
                write_n_fill(stream, fill_ch, pad);
            } else if (p.align == '^') {
                write_n_fill(stream, fill_ch, pad / 2);
            }
            fputc(value, stream);
            if (p.align == '<') {
                write_n_fill(stream, fill_ch, pad);
            } else if (p.align == '^') {
                write_n_fill(stream, fill_ch, pad - pad / 2);
            }
        } else {
            fputc(value, stream);
        }
        return;
    }
    // 否则按整型 (把 char 当 unsigned char 走 b/B/d/o/x/X/c 路径)
    write_argument<unsigned char>(stream, p, static_cast<unsigned char>(value));
}

/*==================================================================*/
/*  bool                                                              */
/*==================================================================*/

inline void write_argument(FILE *stream, const arg_plan &p, const bool &value) {
    if (p.is_string) {
        const char *str = value ? "true" : "false";
        std::size_t len = std::strlen(str);
        if (p.width > 0 && static_cast<int>(len) < p.width) {
            std::size_t pad     = static_cast<std::size_t>(p.width) - len;
            char        fill_ch = p.fill ? p.fill : ' ';
            char        al      = p.align ? p.align : '<';
            if (al == '>') {
                write_n_fill(stream, fill_ch, pad);
            } else if (al == '^') {
                write_n_fill(stream, fill_ch, pad / 2);
            }
            fwrite(str, 1, len, stream);
            if (al == '<') {
                write_n_fill(stream, fill_ch, pad);
            } else if (al == '^') {
                write_n_fill(stream, fill_ch, pad - pad / 2);
            }
        } else {
            fwrite(str, 1, len, stream);
        }
        return;
    }
    // 整型路径: 把 plan 标记 reset, 然后调整型
    write_argument<unsigned char>(stream, p, value ? static_cast<unsigned char>(1)
                                                   : static_cast<unsigned char>(0));
}

/*==================================================================*/
/*  C-string (char[N] 字面量)                                          */
/*==================================================================*/

template <std::size_t N>
inline void write_argument(FILE *stream, const arg_plan &p, const char (&value)[N]) {
    std::size_t length = std::strlen(value);
    if (p.precision >= 0) {
        length = std::min<std::size_t>(length, static_cast<std::size_t>(p.precision));
    }
    if (p.width > 0 && static_cast<int>(length) < p.width) {
        std::size_t pad     = static_cast<std::size_t>(p.width) - length;
        char        fill_ch = p.fill ? p.fill : ' ';
        char        al      = p.align ? p.align : '<';
        if (al == '>') {
            write_n_fill(stream, fill_ch, pad);
        } else if (al == '^') {
            write_n_fill(stream, fill_ch, pad / 2);
        }
        fwrite(value, 1, length, stream);
        if (al == '<') {
            write_n_fill(stream, fill_ch, pad);
        } else if (al == '^') {
            write_n_fill(stream, fill_ch, pad - pad / 2);
        }
    } else {
        fwrite(value, 1, length, stream);
    }
}

/*==================================================================*/
/*  浮点                                                              */
/*==================================================================*/

/* 浮点千分位 noinline 助手 (BUG-08).
 *
 * 完整接管 "thousands + sign + width + zero-pad + align + fill" 输出, 不在 fprint
 * 主路径上展开. 内部用 64 字节栈 buf 装含 ',' 的整段, 加上写外围 pad/fill 所需
 * 的临时变量都在本函数栈帧内, 不会叠加到 fprint 包装器 (fi_f 等) 的栈预算里.
 *
 * 入参:  digits     - to_chars 后的完整字符串 (hex 浮点已 fabs, 无自带 '-')
 *        dlen       - digits 长度
 *        int_len    - 整数位字符数 (0..dlen; 小于 1 的值含前导 0)
 *        sign_ch    - 0 / '+' / '-' / ' '
 */
__attribute__((noinline))
static void write_float_thousands(FILE *stream, const arg_plan &p, const char *digits,
                                  std::size_t dlen, std::size_t int_len, char sign_ch) {
    // 在 64 字节 buf 上组装 [sign][整数 w/ ,][小数/指数]
    // hex 浮点额外把 prefix 插到 sign 之后.
    char        out[64];
    std::size_t olen = 0;
    if (sign_ch) {
        out[olen++] = sign_ch;
    }
    // hex 浮点 prefix 紧跟 sign 后
    if (p.is_hex_float) {
        for (unsigned k = 0; k < p.prefix_len; ++k) {
            out[olen++] = p.prefix[k];
        }
    }
    // 整数位加 ','
    std::size_t first_group = int_len % 3;
    if (first_group == 0 && int_len > 0) {
        first_group = 3;
    }
    std::size_t copied = 0;
    for (std::size_t i = 0; i < int_len; ++i) {
        if (copied == first_group && i != 0) {
            out[olen++] = ',';
            first_group += 3;
        }
        out[olen++] = digits[i];
        ++copied;
    }
    // 剩余: 小数位 / 指数位 / 任何 hex 浮点剩余部分
    for (std::size_t i = int_len; i < dlen; ++i) {
        out[olen++] = digits[i];
    }

    // 输出 (含 sign/prefix/千分位 digits 的最终 blen)
    if (p.width > 0 && static_cast<int>(olen) < p.width) {
        std::size_t pad     = static_cast<std::size_t>(p.width) - olen;
        char        fill_ch = p.fill ? p.fill : ' ';
        char        al      = p.align ? p.align : '>';
        if (p.zero_pad) {
            // zero-pad 模式: sign + prefix + 0 填充 + digits; align 退化为 '>'
            // (与整型 zero-pad 一致)
            if (sign_ch) {
                fputc(sign_ch, stream);
            }
            if (p.is_hex_float) {
                fwrite(p.prefix, 1, p.prefix_len, stream);
            }
            write_n_fill(stream, '0', pad);
            // out[0..sign/prefix 之后] 实际为 digits 部分, 重新写出去
            std::size_t head = (sign_ch ? 1 : 0) + (p.is_hex_float ? p.prefix_len : 0);
            fwrite(out + head, 1, olen - head, stream);
            return;
        }
        if (al == '>') {
            write_n_fill(stream, fill_ch, pad);
        } else if (al == '^') {
            write_n_fill(stream, fill_ch, pad / 2);
        }
        fwrite(out, 1, olen, stream);
        if (al == '<') {
            write_n_fill(stream, fill_ch, pad);
        } else if (al == '^') {
            write_n_fill(stream, fill_ch, pad - pad / 2);
        }
    } else {
        fwrite(out, 1, olen, stream);
    }
}

template <typename T>
    requires std::floating_point<T>
inline void write_argument(FILE *stream, const arg_plan &p, const T &value) {
    // 1. 符号 (BUG-01 + BUG-04 联动)
    // 全部浮点路径统一自己控制 sign_ch, 不依赖 to_chars 内置 '-':
    //   - 负值: sign_ch = '-', to_chars 传 fabs(value) 拿到无 '-' 数字
    //   - 正值: 看 spec.sign 决定 '+' / ' ' / 不写
    // 原因: zero-pad 路径需要 sign 单独写, 然后 '0' 填充, 最后写 digits;
    //       如果 digits 自带 '-', zero-pad 会把 '-' 推到末尾 (错位).
    bool is_negative = std::signbit(value);
    char sign_ch = 0;
    if (is_negative) {
        sign_ch = '-';
    } else if (p.sign_ch == '+') {
        sign_ch = '+';
    } else if (p.sign_ch == ' ') {
        sign_ch = ' ';
    }

    // 2. 精度
    //   - 普通浮点默认 6 (与 std::format 一致)
    //   - hex 浮点默认 1 (std::format 实验: `{:a}` 1.5 → "1.8p+0", 而非 `2p+0`).
    //     精度 0 仍走 "无小数位" 路径 (1.5 → "2p+0").
    int precision      = p.precision;
    bool prec_default  = (precision < 0);
    if (prec_default) {
        precision = p.is_hex_float ? 1 : 6;
    }
    int used_precision = precision;

    // 3. to_chars 到栈 buf 初次尝试，常见情况无需堆分配。
    //    负值传 fabs, 符号已单独走 sign_ch (BUG-01 + BUG-04 联动).
    T    tochars_value = is_negative ? std::fabs(value) : value;
    char small_digits[128];
    char *digits = small_digits;
    std::size_t digit_buf_size = sizeof(small_digits);
    std::string digits_storage;
    auto [q, ec] = std::to_chars(digits, digits + digit_buf_size, tochars_value, p.chars_format, used_precision);
    if (ec != std::errc()) {
        std::size_t need = static_cast<std::size_t>(used_precision) + 64;
        if (need < 256) {
            need = 256;
        }
        digits_storage.resize(need);
        digits = digits_storage.data();
        auto [q2, ec2] = std::to_chars(digits, digits + digits_storage.size(), tochars_value, p.chars_format, used_precision);
        if (ec2 != std::errc()) {
            // thousands 失败退化: 走普通 "0" 退路即可
            if (p.thousands) {
                // 仍然写 0; 不用加 ',' (无整数位)
                std::size_t out_len = sign_ch ? 2 : 1;
                if (p.width > 0 && static_cast<int>(out_len) < p.width) {
                    std::size_t pad     = static_cast<std::size_t>(p.width) - out_len;
                    char        fill_ch = p.fill ? p.fill : ' ';
                    char        al      = p.align ? p.align : '>';
                    if (p.zero_pad) {
                        if (sign_ch) {
                            fputc(sign_ch, stream);
                        }
                        write_n_fill(stream, '0', pad);
                        fputc('0', stream);
                        return;
                    }
                    if (al == '>') {
                        write_n_fill(stream, fill_ch, pad);
                    } else if (al == '^') {
                        write_n_fill(stream, fill_ch, pad / 2);
                    }
                    if (sign_ch) {
                        fputc(sign_ch, stream);
                    }
                    fputc('0', stream);
                    if (al == '<') {
                        write_n_fill(stream, fill_ch, pad);
                    } else if (al == '^') {
                        write_n_fill(stream, fill_ch, pad - pad / 2);
                    }
                } else {
                    if (sign_ch) {
                        fputc(sign_ch, stream);
                    }
                    fputc('0', stream);
                }
                return;
            }
            // 失败: 输出 '0' (或 sign + '0')
            std::size_t out_len = sign_ch ? 2 : 1;
            if (p.width > 0 && static_cast<int>(out_len) < p.width) {
                std::size_t pad     = static_cast<std::size_t>(p.width) - out_len;
                char        fill_ch = p.fill ? p.fill : ' ';
                char        al      = p.align ? p.align : '>';
                if (al == '>') {
                    write_n_fill(stream, fill_ch, pad);
                } else if (al == '^') {
                    write_n_fill(stream, fill_ch, pad / 2);
                }
                if (sign_ch) {
                    fputc(sign_ch, stream);
                }
                fputc('0', stream);
                if (al == '<') {
                    write_n_fill(stream, fill_ch, pad);
                } else if (al == '^') {
                    write_n_fill(stream, fill_ch, pad - pad / 2);
                }
            } else {
                if (sign_ch) {
                    fputc(sign_ch, stream);
                }
                fputc('0', stream);
            }
            return;
        }
        q = q2;
    }
    std::size_t dlen = static_cast<std::size_t>(q - digits);

    // 3.5 thousands 路径: 把整数位长度算出来, 委托 noinline 助手处理
    if (p.thousands) {
        // 整数位 = digits 中第一个 '.' / 'e' / 'E' 之前的字符数.
        // to_chars 对 f/F 永远含 '.'; 对 e/E 含 'e'/'E'; 对 g/G 看精度.
        // hex 浮点 (a/A) 不与 thousands 组合 (std::format 不支持), 此处按失败处理.
        if (p.is_hex_float) {
            // 退化为无 thousands 输出
        } else {
            std::size_t int_len = 0;
            while (int_len < dlen && digits[int_len] != '.' && digits[int_len] != 'e' &&
                   digits[int_len] != 'E' && digits[int_len] != 'n' && digits[int_len] != 'i') {
                // 'n' / 'i' 出现在 "nan" / "inf" / "-nan" / "-inf", 整数位为 0
                ++int_len;
            }
            // 对 "inf" / "nan" 之类 int_len = 0, 不加 ',', 直接走助手
            write_float_thousands(stream, p, digits, dlen, int_len, sign_ch);
            return;
        }
    }

    // 4. 在 digits 中原地做大小写转换.
    if (p.is_hex_float) {
        for (std::size_t i = 0; i < dlen; ++i) {
            char c = digits[i];
            if (p.upper_float) {
                // 大写: 数字 a-f, 指数 p, 以及 inf / nan 出现的 'i' 'n'
                // (a-f 范围已经覆盖 nan 的 'a')
                if (c >= 'a' && c <= 'f') {
                    digits[i] = static_cast<char>(c - 'a' + 'A');
                } else if (c == 'p') {
                    digits[i] = 'P';
                } else if (c == 'i' || c == 'n') {
                    digits[i] = static_cast<char>(c - 'a' + 'A');
                }
            } else if (c == 'P') {
                digits[i] = 'p';
            }
        }
    } else if (p.upper_float) {
        // 处理 inf / nan 的 'i' 'n' 'f' 'a' 大写 + 'e' -> 'E'
        for (std::size_t i = 0; i < dlen; ++i) {
            char c = digits[i];
            if (c == 'e') {
                digits[i] = 'E';
            } else if (c >= 'a' && c <= 'z') {
                digits[i] = static_cast<char>(c - 'a' + 'A');
            }
        }
    }

    // 5. 计算总输出长度: [sign][digits] (hex 浮点 prefix_len=0, 不再额外加)
    std::size_t blen = dlen + (sign_ch ? 1 : 0);

    // 6. zero-pad (sign-aware): sign 占 1 位, 剩余宽度用 '0' 填 (hex 浮点 prefix_len=0)
    if (p.zero_pad && p.width > 0 && static_cast<int>(blen) < p.width) {
        std::size_t pad = static_cast<std::size_t>(p.width) - blen;
        if (sign_ch) {
            fputc(sign_ch, stream);
        }
        write_n_fill(stream, '0', pad);
        fwrite(digits, 1, dlen, stream);
        return;
    }

    // 7. width / fill / align 直接通过 fputc / fwrite 顺序拼装, 不积累到 body[]
    if (p.width > 0 && static_cast<int>(blen) < p.width) {
        std::size_t pad     = static_cast<std::size_t>(p.width) - blen;
        char        fill_ch = p.fill ? p.fill : ' ';
        char        al      = p.align ? p.align : '>';
        if (al == '>') {
            write_n_fill(stream, fill_ch, pad);
        } else if (al == '^') {
            write_n_fill(stream, fill_ch, pad / 2);
        }
        if (sign_ch) {
            fputc(sign_ch, stream);
        }
        fwrite(digits, 1, dlen, stream);
        if (al == '<') {
            write_n_fill(stream, fill_ch, pad);
        } else if (al == '^') {
            write_n_fill(stream, fill_ch, pad - pad / 2);
        }
    } else {
        if (sign_ch) {
            fputc(sign_ch, stream);
        }
        fwrite(digits, 1, dlen, stream);
    }
}

/*==================================================================*/
/*  std::string                                                       */
/*==================================================================*/

inline void write_argument(FILE *stream, const arg_plan &p, const std::string &value) {
    std::size_t length = value.size();
    if (p.precision >= 0) {
        length = std::min<std::size_t>(length, static_cast<std::size_t>(p.precision));
    }
    if (p.width > 0 && static_cast<int>(length) < p.width) {
        std::size_t pad     = static_cast<std::size_t>(p.width) - length;
        char        fill_ch = p.fill ? p.fill : ' ';
        char        al      = p.align ? p.align : '<';
        if (al == '>') {
            write_n_fill(stream, fill_ch, pad);
        } else if (al == '^') {
            write_n_fill(stream, fill_ch, pad / 2);
        }
        fwrite(value.data(), 1, length, stream);
        if (al == '<') {
            write_n_fill(stream, fill_ch, pad);
        } else if (al == '^') {
            write_n_fill(stream, fill_ch, pad - pad / 2);
        }
    } else {
        fwrite(value.data(), 1, length, stream);
    }
}

/*==================================================================*/
/*  std::string_view                                                  */
/*==================================================================*/

inline void write_argument(FILE *stream, const arg_plan &p, const std::string_view &value) {
    std::size_t length = value.size();
    if (p.precision >= 0) {
        length = std::min<std::size_t>(length, static_cast<std::size_t>(p.precision));
    }
    if (p.width > 0 && static_cast<int>(length) < p.width) {
        std::size_t pad     = static_cast<std::size_t>(p.width) - length;
        char        fill_ch = p.fill ? p.fill : ' ';
        char        al      = p.align ? p.align : '<';
        if (al == '>') {
            write_n_fill(stream, fill_ch, pad);
        } else if (al == '^') {
            write_n_fill(stream, fill_ch, pad / 2);
        }
        fwrite(value.data(), 1, length, stream);
        if (al == '<') {
            write_n_fill(stream, fill_ch, pad);
        } else if (al == '^') {
            write_n_fill(stream, fill_ch, pad - pad / 2);
        }
    } else {
        fwrite(value.data(), 1, length, stream);
    }
}

/*==================================================================*/
/*  原始指针                                                          */
/*==================================================================*/

template <typename T>
    requires std::is_pointer_v<T>
inline void write_argument(FILE *stream, const arg_plan &p, const T &value) {
    std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(value);

    // digits[24] + 在 dlen 后追加 prefix (已经在 p.prefix 里)
    // 16-hex uint64 = 16 chars; digits[24] 足够
    char digits[24];
    auto [q, ec] = std::to_chars(digits, digits + sizeof(digits), addr, 16);
    if (ec != std::errc()) {
        // 退路: 写 "0x0"
        std::size_t out_len = p.prefix_len + 1;
        if (p.width > 0 && static_cast<int>(out_len) < p.width) {
            std::size_t pad     = static_cast<std::size_t>(p.width) - out_len;
            char        fill_ch = p.fill ? p.fill : ' ';
            char        al      = p.align ? p.align : '>';
            if (al == '>') {
                write_n_fill(stream, fill_ch, pad);
            } else if (al == '^') {
                write_n_fill(stream, fill_ch, pad / 2);
            }
            fwrite(p.prefix, 1, p.prefix_len, stream);
            fputc('0', stream);
            if (al == '<') {
                write_n_fill(stream, fill_ch, pad);
            } else if (al == '^') {
                write_n_fill(stream, fill_ch, pad - pad / 2);
            }
        } else {
            fwrite(p.prefix, 1, p.prefix_len, stream);
            fputc('0', stream);
        }
        return;
    }
    std::size_t dlen = static_cast<std::size_t>(q - digits);
    if (p.ptr_upper) {
        for (std::size_t i = 0; i < dlen; ++i) {
            char c = digits[i];
            if (c >= 'a' && c <= 'f') {
                digits[i] = static_cast<char>(c - 'a' + 'A');
            }
        }
    }
    std::size_t blen = dlen + p.prefix_len;

    if (p.width > 0 && static_cast<int>(blen) < p.width) {
        std::size_t pad     = static_cast<std::size_t>(p.width) - blen;
        char        fill_ch = p.fill ? p.fill : ' ';
        char        al      = p.align ? p.align : '>';
        if (al == '>') {
            write_n_fill(stream, fill_ch, pad);
        } else if (al == '^') {
            write_n_fill(stream, fill_ch, pad / 2);
        }
        fwrite(p.prefix, 1, p.prefix_len, stream);
        fwrite(digits, 1, dlen, stream);
        if (al == '<') {
            write_n_fill(stream, fill_ch, pad);
        } else if (al == '^') {
            write_n_fill(stream, fill_ch, pad - pad / 2);
        }
    } else {
        fwrite(p.prefix, 1, p.prefix_len, stream);
        fwrite(digits, 1, dlen, stream);
    }
}

/*==================================================================*/
/*  nullptr_t                                                         */
/*==================================================================*/

inline void write_argument(FILE *stream, const arg_plan &p, const std::nullptr_t &) {
    // "0x0" → 3 bytes; 无 prefix 时只写 "0"
    std::size_t blen = p.prefix_len + 1;

    if (p.width > 0 && static_cast<int>(blen) < p.width) {
        std::size_t pad     = static_cast<std::size_t>(p.width) - blen;
        char        fill_ch = p.fill ? p.fill : ' ';
        char        al      = p.align ? p.align : '>';
        if (al == '>') {
            write_n_fill(stream, fill_ch, pad);
        } else if (al == '^') {
            write_n_fill(stream, fill_ch, pad / 2);
        }
        fwrite(p.prefix, 1, p.prefix_len, stream);
        fputc('0', stream);
        if (al == '<') {
            write_n_fill(stream, fill_ch, pad);
        } else if (al == '^') {
            write_n_fill(stream, fill_ch, pad - pad / 2);
        }
    } else {
        fwrite(p.prefix, 1, p.prefix_len, stream);
        fputc('0', stream);
    }
}

template <typename T>
inline void write_argument(FILE *, const arg_plan &, const T &) = delete;

} // namespace em
