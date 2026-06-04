// em_fmt 全功能展示 (test_func.cpp)
// 一行一个独立功能, 用 stdout 输出, 编译运行后可直接肉眼 / diff 对照。
#include "em_fmt.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

int main() {
    // === 0. 基础字面量 / 空格式串 / 转义花括号 ===
    em::fprint<"hello, em_fmt\n">(stdout);
    em::fprint<"{{ escaped braces: {{}} }}\n">(stdout);
    em::fprint<"a{{b{}c}}d\n">(stdout, 123);
    em::fprint<"empty arg: [{}]\n">(stdout, 0);

    // === 1. 整型 — 默认 / 类型字符 ===
    em::fprint<"[d]    42={:d}\n">(stdout, 42);
    em::fprint<"[-42]  ={:d}\n">(stdout, -42);
    em::fprint<"[b]    42={:b}\n">(stdout, 42);
    em::fprint<"[B]    42={:B}\n">(stdout, 42);
    em::fprint<"[o]    42={:o}\n">(stdout, 42);
    em::fprint<"[x]    255={:x}\n">(stdout, 255);
    em::fprint<"[X]    255={:X}\n">(stdout, 255);
    em::fprint<"[c]    65={:c}\n">(stdout, 65);

    // === 2. 整型 — 替代形式 '#' ===
    em::fprint<"[#b]   42={:#b}\n">(stdout, 42);
    em::fprint<"[#B]   42={:#B}\n">(stdout, 42);
    em::fprint<"[#o]   42={:#o}\n">(stdout, 42);
    em::fprint<"[#o]   0 ={:#o}\n">(stdout, 0);
    em::fprint<"[#x]   255={:#x}\n">(stdout, 255);
    em::fprint<"[#X]   255={:#X}\n">(stdout, 255);
    em::fprint<"[#x]   0 ={:#x}\n">(stdout, 0);
    em::fprint<"[x]    0 ={:x}\n">(stdout, 0);

    // === 3. 整型 — 符号控制 ===
    em::fprint<"[+]    42 ={:+d}\n">(stdout, 42);
    em::fprint<"[+]   -42 ={:+d}\n">(stdout, -42);
    em::fprint<"[ ]    42 ={: d}\n">(stdout, 42);
    em::fprint<"[ ]   -42 ={: d}\n">(stdout, -42);
    em::fprint<"[def] -42 ={:d}\n">(stdout, -42);
    em::fprint<"[def]  42 ={:d}\n">(stdout, 42);

    // === 4. 整型 — 零填充 (sign-aware) ===
    em::fprint<"[05d]   42 ={:05d}\n">(stdout, 42);
    em::fprint<"[05d]  -42 ={:05d}\n">(stdout, -42);
    em::fprint<"[+05d]  42 ={:+05d}\n">(stdout, 42);
    em::fprint<"[ 05d]  42 ={: 05d}\n">(stdout, 42);
    em::fprint<"[#08x]  255={:#08x}\n">(stdout, 255);

    // === 5. 整型 — 宽度 + 对齐 + 自定义 fill ===
    em::fprint<"[5d]    42 ={:5d}\n">(stdout, 42);
    em::fprint<"[<5d]   42 ={:<5d}\n">(stdout, 42);
    em::fprint<"[>5d]   42 ={:>5d}\n">(stdout, 42);
    em::fprint<"[^5d]   42 ={:^5d}\n">(stdout, 42);
    em::fprint<"[*<5d]  42 ={:*<5d}\n">(stdout, 42);
    em::fprint<"[*>5d]  42 ={:*>5d}\n">(stdout, 42);
    em::fprint<"[*^5d]  42 ={:*^5d}\n">(stdout, 42);
    em::fprint<"[0<5d]  42 ={:0<5d}\n">(stdout, 42);
    em::fprint<"[0>5d]  42 ={:0>5d}\n">(stdout, 42);

    // === 6. 整型 — 千分位 ',' ===
    em::fprint<"[,d]  1234567 ={:,d}\n">(stdout, 1234567);
    em::fprint<"[,d]  1000     ={:,d}\n">(stdout, 1000);
    em::fprint<"[,d]  0        ={:,d}\n">(stdout, 0);
    em::fprint<"[+10,d] 1234567 ={:+10,d}\n">(stdout, 1234567);

    // === 7. 各种整型宽度 ===
    em::fprint<"[int8]   -5 ={:d}\n">(stdout, static_cast<int8_t>(-5));
    em::fprint<"[uint8]  255={:d}\n">(stdout, static_cast<uint8_t>(255));
    em::fprint<"[int16]  -1 ={:d}\n">(stdout, static_cast<int16_t>(-1));
    em::fprint<"[uint16] 65535={:d}\n">(stdout, static_cast<uint16_t>(65535));
    em::fprint<"[int32]  -1 ={:d}\n">(stdout, static_cast<int32_t>(-1));
    em::fprint<"[uint32] 4294967295={:d}\n">(stdout, static_cast<uint32_t>(4294967295u));
    em::fprint<"[int64]  -1 ={:d}\n">(stdout, static_cast<int64_t>(-1));
    em::fprint<"[uint64] 18446744073709551615={:d}\n">(stdout,
                                                     std::numeric_limits<uint64_t>::max());
    em::fprint<"[size_t] 123456 ={:d}\n">(stdout, static_cast<size_t>(123456));

    // === 8. 字符 / 整数当字符 ===
    em::fprint<"[c]    'A' ={:c}\n">(stdout, 'A');
    em::fprint<"[c]    66  ={:c}\n">(stdout, 66);
    em::fprint<"[5c]   'A' ={:5c}\n">(stdout, 'A');
    em::fprint<"[*<5c] 'A' ={:*<5c}\n">(stdout, 'A');

    // === 9. 字符串 — 基础 / 宽度 / 对齐 / fill ===
    em::fprint<"[s]     'hi' ={:s}\n">(stdout, "hi");
    em::fprint<"[5s]    'hi' ={:5s}\n">(stdout, "hi");
    em::fprint<"[<5s]   'hi' ={:<5s}\n">(stdout, "hi");
    em::fprint<"[>5s]   'hi' ={:>5s}\n">(stdout, "hi");
    em::fprint<"[^5s]   'hi' ={:^5s}\n">(stdout, "hi");
    em::fprint<"[*<5s]  'hi' ={:*<5s}\n">(stdout, "hi");
    em::fprint<"[*>5s]  'hi' ={:*>5s}\n">(stdout, "hi");
    em::fprint<"[*^7s]  'hi' ={:*^7s}\n">(stdout, "hi");

    // === 10. 字符串 — 精度截断 ===
    em::fprint<"[.3s]   'hello' ={:.3s}\n">(stdout, "hello");
    em::fprint<"[.0s]   'hello' ={:.0s}\n">(stdout, "hello");
    em::fprint<"[5.3s]  'hello' ={:5.3s}\n">(stdout, "hello");
    em::fprint<"[8.3s]  'hi'    ={:8.3s}\n">(stdout, "hi");
    em::fprint<"[.10s]  'hi'    ={:.10s}\n">(stdout, "hi");

    // === 11. std::string ===
    em::fprint<"[s]    std::string ={:s}\n">(stdout, std::string("std::string"));
    em::fprint<"[8s]   std::string ={:8s}\n">(stdout, std::string("std::string"));
    em::fprint<"[.5s]  std::string ={:.5s}\n">(stdout, std::string("std::string"));
    em::fprint<"[<10s] std::string ={:<10s}\n">(stdout, std::string("std::string"));

    // === 12. std::string_view ===
    em::fprint<"[s]    string_view={:s}\n">(stdout, std::string_view("std::string_view"));
    em::fprint<"[10s]  string_view={:10s}\n">(stdout, std::string_view("std::string_view"));
    em::fprint<"[.5s]  string_view={:.5s}\n">(stdout, std::string_view("std::string_view"));

    // === 13. 布尔 — 字符串路径 ===
    em::fprint<"[s] true ={:s}\n">(stdout, true);
    em::fprint<"[s] false={:s}\n">(stdout, false);
    em::fprint<"[<8s]true ={:<8s}\n">(stdout, true);
    em::fprint<"[>8s]true ={:>8s}\n">(stdout, true);

    // === 14. 布尔 — 整型路径 ===
    em::fprint<"[d] true ={:d}\n">(stdout, true);
    em::fprint<"[d] false={:d}\n">(stdout, false);
    em::fprint<"[x] true ={:x}\n">(stdout, true);
    em::fprint<"[X] true ={:X}\n">(stdout, true);
    em::fprint<"[#x]true ={:#x}\n">(stdout, true);
    em::fprint<"[o] true ={:o}\n">(stdout, true);
    em::fprint<"[b] true ={:b}\n">(stdout, true);

    // === 15. 指针 / nullptr ===
    {
        void *p1 = reinterpret_cast<void *>(std::uintptr_t{ 0x1234ABCD });
        void *p2 = reinterpret_cast<void *>(std::uintptr_t{ 0xDEADBEEF });
        em::fprint<"[p] 0x1234ABCD ={:p}\n">(stdout, p1);
        em::fprint<"[P] 0xDEADBEEF ={:P}\n">(stdout, p2);
        em::fprint<"[p] nullptr    ={:p}\n">(stdout, nullptr);
        em::fprint<"[20p] 0x1234ABCD ={:>20p}\n">(stdout, p1);
        em::fprint<"[#P] 0x1234ABCD ={:#P}\n">(stdout, p1);
    }

    // === 16. 浮点 — 默认 / 类型字符 ===
    em::fprint<"[g]  3.14159  ={:g}\n">(stdout, 3.14159);
    em::fprint<"[G]  3.14159  ={:G}\n">(stdout, 3.14159);
    em::fprint<"[e]  123.456  ={:e}\n">(stdout, 123.456);
    em::fprint<"[E]  123.456  ={:E}\n">(stdout, 123.456);
    em::fprint<"[f]  3.14159  ={:f}\n">(stdout, 3.14159);
    em::fprint<"[F]  3.14159  ={:F}\n">(stdout, 3.14159);
    em::fprint<"[a]  1.5      ={:a}\n">(stdout, 1.5);
    em::fprint<"[A]  1.5      ={:A}\n">(stdout, 1.5);

    // === 17. 浮点 — 精度 ===
    em::fprint<"[.2f] 3.14159 ={:.2f}\n">(stdout, 3.14159);
    em::fprint<"[.5f] 3.14    ={:.5f}\n">(stdout, 3.14);
    em::fprint<"[.0f] 3.14    ={:.0f}\n">(stdout, 3.14);
    em::fprint<"[.3e] 123.456 ={:.3e}\n">(stdout, 123.456);
    em::fprint<"[.3g] 3.14159 ={:.3g}\n">(stdout, 3.14159);
    em::fprint<"[.10a] 1.5     ={:.10a}\n">(stdout, 1.5);

    // === 18. 浮点 — 符号 ===
    em::fprint<"[+]  1.5 ={:+f}\n">(stdout, 1.5);
    em::fprint<"[+] -1.5 ={:+f}\n">(stdout, -1.5);
    em::fprint<"[ ]  1.5 ={: f}\n">(stdout, 1.5);
    em::fprint<"[ ] -1.5 ={: f}\n">(stdout, -1.5);
    em::fprint<"[def] 1.5 ={:f}\n">(stdout, 1.5);

    // === 19. 浮点 — 宽度 / 对齐 / fill / 零填充 ===
    em::fprint<"[10f]  3.14 ={:10f}\n">(stdout, 3.14);
    em::fprint<"[<10f] 3.14 ={:<10f}\n">(stdout, 3.14);
    em::fprint<"[>10f] 3.14 ={:>10f}\n">(stdout, 3.14);
    em::fprint<"[^10f] 3.14 ={:^10f}\n">(stdout, 3.14);
    em::fprint<"[*<10f] 3.14 ={:*<10f}\n">(stdout, 3.14);
    em::fprint<"[*^10f] 3.14 ={:*^10f}\n">(stdout, 3.14);
    em::fprint<"[010.2f] 3.14 ={:010.2f}\n">(stdout, 3.14);
    em::fprint<"[+08.2f] 3.14 ={:+08.2f}\n">(stdout, 3.14);

    // === 20. 浮点 — 特殊值 ===
    em::fprint<"[F] inf ={:F}\n">(stdout, std::numeric_limits<double>::infinity());
    em::fprint<"[f] inf ={:f}\n">(stdout, std::numeric_limits<double>::infinity());
    em::fprint<"[F] nan ={:F}\n">(stdout, std::numeric_limits<double>::quiet_NaN());
    em::fprint<"[f] nan ={:f}\n">(stdout, std::numeric_limits<double>::quiet_NaN());
    em::fprint<"[e] 0   ={:e}\n">(stdout, 0.0);
    em::fprint<"[f] 0   ={:f}\n">(stdout, 0.0);
    em::fprint<"[g] 0   ={:g}\n">(stdout, 0.0);

    // === 21. 浮点 — float (单精度) ===
    em::fprint<"[.3f] float 3.14 ={:.3f}\n">(stdout, 2.71828f);

    // === 22. 浮点 — 大 / 极小值 ===
    em::fprint<"[e] 1e100 ={:e}\n">(stdout, 1e100);
    em::fprint<"[e] 1e-30 ={:e}\n">(stdout, 1e-30);
    em::fprint<"[g] 1e20  ={:g}\n">(stdout, 1e20);

    // === 23. 复合 — fill + align + sign + width + type ===
    em::fprint<"[*<+8d]   42 ={:*<+8d}\n">(stdout, 42);
    em::fprint<"[*<+8d]  -42 ={:*<+8d}\n">(stdout, -42);
    em::fprint<"[*>+8d]    42 ={:*>+8d}\n">(stdout, 42);
    em::fprint<"[*^+8d]    42 ={:*^+8d}\n">(stdout, 42);
    em::fprint<"[*<+10x]  255 ={:*<+10x}\n">(stdout, 255);
    em::fprint<"[*^12s]    'hi' ={:*^12s}\n">(stdout, "hi");

    // === 24. 顺序、混合字面量与多个 arg ===
    em::fprint<"a{}b{}c{}\n">(stdout, 1, 2, 3);
    em::fprint<"[{:d}]-[{:x}]-[{:b}]\n">(stdout, 255, 255, 255);
    em::fprint<"{:s}={:d} {:#x} {:c} {:s}\n">(stdout,
                                            "key",
                                            42,
                                            255,
                                            '!',
                                            true);

    // === 25. 边界 — 0 / 负零 / 空串 ===
    em::fprint<"[d] 0    ={:d}\n">(stdout, 0);
    em::fprint<"[d] INT_MIN={:d}\n">(stdout, std::numeric_limits<int>::min());
    em::fprint<"[d] INT_MAX={:d}\n">(stdout, std::numeric_limits<int>::max());
    em::fprint<"[f] -0.0 ={:f}\n">(stdout, -0.0);
    em::fprint<"[s] ''   =[{:s}]\n">(stdout, "");
    em::fprint<"[c] 0    =[{:c}]\n">(stdout, '\0');

    return 0;
}
