// SPDX-License-Identifier: MIT
// em_fmt 标准格式控制 —— 运行时正确性测试
//
// 策略: 凡是 std::format 应当产生相同输出的用例, 我们直接拿 std::format
// 的编译期 format_string 当真值。这样 em_fmt 必须与 C++23 std::format
// 行为一致 —— 不需要手抄期望字符串。
//
// 只有 em_fmt 自身扩展 (例如 {:c} 接受 int、{:#x} 接受 bool、{:P} 大写
// 指针、{:s} 接受 bool) 才使用手写期望, 这些行为 std::format 并不支持。
#include "em_fmt.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <limits>
#include <format>
#include <string>
#include <string_view>

namespace {

constexpr const char *kTempPath = "/tmp/em_fmt_spec_test.out";

struct Capture {
    Capture() { std::freopen(kTempPath, "w+", stdout); }
    ~Capture() { finish(); }
    void finish() {
        std::fflush(stdout);
        std::fclose(stdout);
    }
    std::string read_all() {
        finish();
        FILE *f = std::fopen(kTempPath, "rb");
        if (!f) return {};
        std::string out;
        char buf[4096];
        std::size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
            out.append(buf, n);
        }
        std::fclose(f);
        return out;
    }
};

int g_failures = 0;

// ---------------------------------------------------------------------------
// EXPECT_OUTPUT(EXPECTED_EXPR, FMT, ...)
//   EXPECTED_EXPR 是 const char* 或 std::string, 用作真值。
//   FMT 必须是编译期字符串字面量 (同时给 std::format 和 em::fprint 用)。
//
// 绝大多数用例里 EXPECTED_EXPR 直接写 vfmt("...", ...), 由编译器
// 在编译期生成期望值, 运行期只做字符串比较。
// ---------------------------------------------------------------------------

// 用 std::vformat 在运行时生成期望值;这样调用点不必出现在 consteval
// 上下文 (而 std::format 的第一参数是 consteval 转换, 在宏展开的赋值
// 语句里会失败)。std::vformat 接收 string_view + format_args, 后者由
// std::make_format_args<...> 包装, 而后者要求参数是 lvalue, 所以先把
// 实参 decay 后存到 std::tuple, 再逐个 lvalue 取出。
template <typename Tuple, std::size_t... Is>
std::string vfmt_apply(const char *literal, Tuple &&t,
                       std::index_sequence<Is...>) {
    return std::vformat(
        std::string_view(literal),
        std::make_format_args(std::get<Is>(t)...));
}

template <typename... Args>
std::string vfmt(const char *literal, Args &&...args) {
    // tuple 的元素类型 decay 掉引用/cv-qual,保证里面都是值类型,lvalue。
    std::tuple<std::decay_t<Args>...> t(std::forward<Args>(args)...);
    return vfmt_apply(literal, t,
                      std::index_sequence_for<Args...>{});
}

#define EXPECT_OUTPUT(EXPECTED, FMT, ...)                                  \
    do {                                                                   \
        Capture cap;                                                       \
        em::fprint<FMT>(stdout __VA_OPT__(,) __VA_ARGS__);                 \
        std::string got  = cap.read_all();                                 \
        std::string want = (EXPECTED);                                     \
        if (got != want) {                                                 \
            ++g_failures;                                                  \
            std::fprintf(stderr,                                           \
                         "MISMATCH at %s:%d\n  want (%zu): %s\n  got  "     \
                         "(%zu): %s\n",                                    \
                         __FILE__, __LINE__, want.size(),                  \
                         want.c_str(), got.size(), got.c_str());           \
        }                                                                  \
    } while (0)

} // namespace

int main() {
    // --- width / align / fill -----------------------------------------------
    EXPECT_OUTPUT(vfmt("hello {:5d}",   42), "hello {:5d}",   42);
    EXPECT_OUTPUT(vfmt("hello {:<5d}",  42), "hello {:<5d}",  42);
    EXPECT_OUTPUT(vfmt("hello {:>5d}",  42), "hello {:>5d}",  42);
    EXPECT_OUTPUT(vfmt("hello {:^5d}",  42), "hello {:^5d}",  42);
    EXPECT_OUTPUT(vfmt("hello {:_^5d}", 42), "hello {:_^5d}", 42);
    EXPECT_OUTPUT(vfmt("hello {:*<5d}", 42), "hello {:*<5d}", 42);

    // --- sign ---------------------------------------------------------------
    EXPECT_OUTPUT(vfmt("x {:+d}",  42),  "x {:+d}",  42);
    EXPECT_OUTPUT(vfmt("x {:+d}", -42),  "x {:+d}", -42);
    EXPECT_OUTPUT(vfmt("x {: d}",  42),  "x {: d}",  42);
    EXPECT_OUTPUT(vfmt("x {: d}",   5),  "x {: d}",   5);
    EXPECT_OUTPUT(vfmt("x {: d}",  -5),  "x {: d}",  -5);

    // --- '0' zero-pad (sign-aware) -----------------------------------------
    EXPECT_OUTPUT(vfmt("x {:05d}",   42),  "x {:05d}",   42);
    EXPECT_OUTPUT(vfmt("x {:05d}",  -42),  "x {:05d}",  -42);
    EXPECT_OUTPUT(vfmt("x {:+05d}",  42),  "x {:+05d}",  42);
    EXPECT_OUTPUT(vfmt("x {: 05d}",  42),  "x {: 05d}",  42);

    // --- 0 + align: '0' is treated as fill when used with align ------------
    EXPECT_OUTPUT(vfmt("x {:0<5d}", 42),   "x {:0<5d}", 42);

    // --- BUG-05: width 解析溢出必须不产生 UB -------------------------------
    // std::format / printf 对超大 width 通常 clamp; em_fmt 之前用 int 累加
    // 会整数溢出 (UB)。这里只放一行正常 width(8) 用例确保 consteval clamp
    // 不破坏已有路径。
    EXPECT_OUTPUT(vfmt("x {:08d}", 42),    "x {:08d}", 42);

    // --- type chars for integers --------------------------------------------
    EXPECT_OUTPUT(vfmt("x {:d}",   42),    "x {:d}",   42);
    EXPECT_OUTPUT(vfmt("x {:#b}",  42),    "x {:#b}",  42);
    EXPECT_OUTPUT(vfmt("x {:#B}",  42),    "x {:#B}",  42);
    EXPECT_OUTPUT(vfmt("x {:#o}",  42),    "x {:#o}",  42);
    EXPECT_OUTPUT(vfmt("x {:#x}",  42),    "x {:#x}",  42);
    EXPECT_OUTPUT(vfmt("x {:#X}",  42),    "x {:#X}",  42);
    EXPECT_OUTPUT(vfmt("x {:X}",   42),    "x {:X}",   42);   // BUG-02
    EXPECT_OUTPUT(vfmt("x {:x}",    0),    "x {:x}",    0);   // BUG-02
    EXPECT_OUTPUT(vfmt("x {:b}",    0),    "x {:b}",    0);
    EXPECT_OUTPUT(vfmt("x {:b}",  1ULL << 63), "x {:b}",  1ULL << 63);
    EXPECT_OUTPUT(vfmt("x {:#b}", 1ULL << 63), "x {:#b}", 1ULL << 63);

    // --- BUG-02: x/b/X/B 无 alt-form 时不应带 prefix -----------------------
    EXPECT_OUTPUT(vfmt("x {:x}",  255),    "x {:x}",  255);
    EXPECT_OUTPUT(vfmt("x {:X}",  255),    "x {:X}",  255);
    EXPECT_OUTPUT(vfmt("x {:b}",   42),    "x {:b}",   42);
    EXPECT_OUTPUT(vfmt("x {:B}",   42),    "x {:B}",   42);
    EXPECT_OUTPUT(vfmt("x {:x}",    0),    "x {:x}",    0);
    EXPECT_OUTPUT(vfmt("x {:b}",    0),    "x {:b}",    0);
    EXPECT_OUTPUT(vfmt("x {:o}",    0),    "x {:o}",    0);
    // 带 alt-form 的回归
    EXPECT_OUTPUT(vfmt("x {:#b}",  42),    "x {:#b}",  42);
    EXPECT_OUTPUT(vfmt("x {:#B}",  42),    "x {:#B}",  42);
    EXPECT_OUTPUT(vfmt("x {:#o}",  42),    "x {:#o}",  42);
    EXPECT_OUTPUT(vfmt("x {:#x}",  42),    "x {:#x}",  42);
    EXPECT_OUTPUT(vfmt("x {:#X}",  42),    "x {:#X}",  42);
    EXPECT_OUTPUT(vfmt("x {:#x}",   0),    "x {:#x}",   0);
    EXPECT_OUTPUT(vfmt("x {:#o}",   8),    "x {:#o}",   8);

    // --- BUG-03: {:#b} 0 必须保留 0b prefix ---------------------------------
    EXPECT_OUTPUT(vfmt("x {:#b}",  0),     "x {:#b}",  0);
    // 同时 {:#o} 0 抑制 (0 ≡ 00) 应保持
    EXPECT_OUTPUT(vfmt("x {:#o}",  0),     "x {:#o}",  0);

    // --- BUG-06: 千分位 zero-pad 与 width 组合 ------------------------------
    // em_fmt 扩展: 千分位 `,` (std::format 接受但需要 locale; 我们在无
    // locale 上下文里手写期望)。
    EXPECT_OUTPUT("x 1,234,567",                      "x {:,d}",     1234567);
    EXPECT_OUTPUT("x     1,234,567",                  "x {:>13,d}",  1234567);
    EXPECT_OUTPUT("x +1,234,567",                     "x {:+,d}",    1234567);
    EXPECT_OUTPUT("x -1,234,567",                     "x {:,d}",    -1234567);
    // em_fmt 扩展: {:0,n} (zero-pad + thousands) 是 std::format 不接受的。
    EXPECT_OUTPUT("x +1,234,567",                     "x {:+08,d}",  1234567);

    // --- char ---------------------------------------------------------------
    // em_fmt 扩展: {:c} 接受 int (std::format 不支持), 手写期望。
    EXPECT_OUTPUT("x A",   "x {:c}", 'A');
    EXPECT_OUTPUT("x A",   "x {:c}", static_cast<int>(65));
    // BUG-01 fix: char 默认左对齐 (与 std::format 一致)。
    EXPECT_OUTPUT(vfmt("x {:5c}", 'A'), "x {:5c}", 'A');

    // --- string width / truncation -----------------------------------------
    EXPECT_OUTPUT(vfmt("x {:5s}",   "ab"),  "x {:5s}",   "ab");
    EXPECT_OUTPUT(vfmt("x {:>5s}",  "ab"),  "x {:>5s}",  "ab");
    EXPECT_OUTPUT(vfmt("x {:*<5s}", "ab"),  "x {:*<5s}", "ab");
    EXPECT_OUTPUT(vfmt("x {:.3s}",  "hello"), "x {:.3s}", "hello");
    EXPECT_OUTPUT(vfmt("x {:5.3s}", "hello"), "x {:5.3s}", "hello");
    EXPECT_OUTPUT(vfmt("x {:8.5s}", "hello"), "x {:8.5s}", "hello");

    // --- std::string / std::string_view ------------------------------------
    EXPECT_OUTPUT(vfmt("x {:5s}", std::string("ab")),      "x {:5s}", std::string("ab"));
    EXPECT_OUTPUT(vfmt("x {:5s}", std::string_view("ab")),  "x {:5s}", std::string_view("ab"));
    EXPECT_OUTPUT(vfmt("x {:.3s}", std::string("hello")),   "x {:.3s}", std::string("hello"));
    EXPECT_OUTPUT(vfmt("x {:.3s}", std::string_view("hello")), "x {:.3s}", std::string_view("hello"));

    // --- bool ---------------------------------------------------------------
    // em_fmt 扩展: {:s} 接受 bool 输出 "true"/"false"; {:d} 接受 bool 输出 0/1。
    // std::format 不接受 bool 给 :s/:d, 必须手写期望。
    EXPECT_OUTPUT("x true",     "x {:s}", true);
    EXPECT_OUTPUT("x false",    "x {:s}", false);
    EXPECT_OUTPUT("x    true",  "x {:>7s}", true);
    EXPECT_OUTPUT("x 1",        "x {:d}", true);
    EXPECT_OUTPUT("x 0",        "x {:d}", false);
    // em_fmt 扩展: {:#x} 接受 bool。
    EXPECT_OUTPUT("x 0x1",      "x {:#x}", true);

    // --- BUG-01: 浮点 hex ({:a} / {:A}) 符号必须前缀在 0x 之前 --------------
    EXPECT_OUTPUT(vfmt("x {:a}",  1.5),  "x {:a}",  1.5);
    EXPECT_OUTPUT(vfmt("x {:a}", -1.5),  "x {:a}", -1.5);
    EXPECT_OUTPUT(vfmt("x {:+a}",  1.5), "x {:+a}",  1.5);
    EXPECT_OUTPUT(vfmt("x {:+a}", -1.5), "x {:+a}", -1.5);
    EXPECT_OUTPUT(vfmt("x {: a}",  1.5), "x {: a}",  1.5);
    EXPECT_OUTPUT(vfmt("x {: a}", -1.5), "x {: a}", -1.5);
    EXPECT_OUTPUT(vfmt("x {:A}",  -1.5), "x {:A}",  -1.5);
    EXPECT_OUTPUT(vfmt("x {:>10a}", -1.5), "x {:>10a}", -1.5);
    EXPECT_OUTPUT(vfmt("x {:>10a}",  1.5), "x {:>10a}",  1.5);

    // --- BUG-04: 浮点 zero-pad 必须是 '0' 填充 -----------------------------
    EXPECT_OUTPUT(vfmt("x {:010.2f}",  3.14),  "x {:010.2f}",  3.14);
    EXPECT_OUTPUT(vfmt("x {:+08.2f}",  3.14),  "x {:+08.2f}",  3.14);
    EXPECT_OUTPUT(vfmt("x {:08.2f}",  -3.14),  "x {:08.2f}",  -3.14);
    EXPECT_OUTPUT(vfmt("x {: 08.2f}",  3.14),  "x {: 08.2f}",  3.14);
    EXPECT_OUTPUT(vfmt("x {:012.2f}",  3.14),  "x {:012.2f}",  3.14);
    EXPECT_OUTPUT(vfmt("x {:010.2f}", -3.14),  "x {:010.2f}", -3.14);

    // --- pointer / nullptr -------------------------------------------------
    void *p = reinterpret_cast<void *>(std::uintptr_t{ 0x1234ABCD });
    // em_fmt 扩展: {:P} 大写指针, std::format 不支持。
    // em_fmt 扩展: 指针格式化 (std::format 不接受 void*), 手写期望。
    EXPECT_OUTPUT("x 0x1234abcd",                  "x {:p}", p);
    EXPECT_OUTPUT("x 0X1234ABCD",                  "x {:P}", p);
    EXPECT_OUTPUT("x 0x0",                         "x {:p}", nullptr);
    EXPECT_OUTPUT("x      0x1234abcd",             "x {:>15p}", p);

    // --- float types -------------------------------------------------------
    EXPECT_OUTPUT(vfmt("x {:.2f}",  3.14159),  "x {:.2f}",  3.14159);
    EXPECT_OUTPUT(vfmt("x {:.3f}",  3.14159),  "x {:.3f}",  3.14159);
    EXPECT_OUTPUT(vfmt("x {:.2f}",  3.14),     "x {:.2f}",  3.14);
    EXPECT_OUTPUT(vfmt("x {:.20f}", 1.5),      "x {:.20f}", 1.5);
    EXPECT_OUTPUT(vfmt("x {:.100f}", 1.5),     "x {:.100f}", 1.5);
    EXPECT_OUTPUT(vfmt("x {:.0f}",  1e100),    "x {:.0f}",  1e100);
    EXPECT_OUTPUT(vfmt("x {:e}",   123.456),   "x {:e}",   123.456);
    EXPECT_OUTPUT(vfmt("x {:E}",   123.456),   "x {:E}",   123.456);
    EXPECT_OUTPUT(vfmt("x {:g}",   3.14159),   "x {:g}",   3.14159);
    // hex-float {:A} / {:a} 必须正确大小写 inf / nan
    EXPECT_OUTPUT(vfmt("x {:F}", std::numeric_limits<double>::infinity()),
                  "x {:F}", std::numeric_limits<double>::infinity());
    EXPECT_OUTPUT(vfmt("x {:F}", std::numeric_limits<double>::quiet_NaN()),
                  "x {:F}", std::numeric_limits<double>::quiet_NaN());
    EXPECT_OUTPUT(vfmt("x {:A}", std::numeric_limits<double>::infinity()),
                  "x {:A}", std::numeric_limits<double>::infinity());
    EXPECT_OUTPUT(vfmt("x {:A}", std::numeric_limits<double>::quiet_NaN()),
                  "x {:A}", std::numeric_limits<double>::quiet_NaN());
    EXPECT_OUTPUT(vfmt("x {:a}", std::numeric_limits<double>::infinity()),
                  "x {:a}", std::numeric_limits<double>::infinity());
    EXPECT_OUTPUT(vfmt("x {:a}", std::numeric_limits<double>::quiet_NaN()),
                  "x {:a}", std::numeric_limits<double>::quiet_NaN());
    EXPECT_OUTPUT(vfmt("x {:+f}",  1.5),      "x {:+f}",  1.5);
    EXPECT_OUTPUT(vfmt("x {:+.3f}", 1.5),     "x {:+.3f}", 1.5);

    // --- escaped braces ----------------------------------------------------
    // 用 vfmt 锁期望值, 两边 (std::format 与 em_fmt) 用同一份 FMT 跑。
    EXPECT_OUTPUT(vfmt("{{{{x}}}}"),              "{{{{x}}}}");
    EXPECT_OUTPUT(vfmt("a{{}}"),                  "a{{}}");
    EXPECT_OUTPUT(vfmt("{{{:d}}}x{{}}", 42),      "{{{:d}}}x{{}}", 42);

    // --- 报告 --------------------------------------------------------------
    if (g_failures == 0) {
        std::fprintf(stderr, "[test_spec] all assertions passed\n");
        return 0;
    }
    std::fprintf(stderr, "[test_spec] %d assertion(s) failed\n", g_failures);
    return 1;
}
