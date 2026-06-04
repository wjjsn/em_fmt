// test_speed.cpp —— 4 路径 throughput 基准, 难度对等.
//
// 设计目标:
//   1. 四个被测方法做完全相同的事 —— 同样的字段数、同样的参数、同样的输出
//      字节数, 跑同样的迭代次数. 差别只在"格式化机制".
//   2. 每次循环都从外部数组读参数, 禁止编译器常量折叠 / 死代码消除.
//   3. 输出重定向到 /dev/null, 排除终端驱动 / 同步锁等"非格式化"成本.
//   4. 四种格式串表达同一种语义 —— 5 字段:
//         十进制 / 十六进制 alt-form / 浮点精度 / 字符串 / 字符
//      每个方法用自己的语法写一遍, 字段顺序和类型对应关系一致.
//
// 负载: 100 (外层) × 1145 (内层) = 114500 次格式化调用, 每次 5 字段.

#include "em_fmt.hpp"

#include <cstddef>
#include <cstdio>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <print>
#include <string>
#include <utility>

namespace {

// 预填的"输入池": 编译器无法在编译期算出来, 必须在运行期从内存读.
// 5 个字段 × 相同长度, 与 4 路径的"每次循环读 5 个值"对齐.
struct Input {
    int          i;      // 字段 1: 十进制
    unsigned     x;      // 字段 2: 十六进制 alt-form
    double       f;      // 字段 3: 浮点 .3f
    std::string  s;      // 字段 4: 字符串
    char         c;      // 字段 5: 字符
};

constexpr std::size_t kInputs = 4096;

Input*      g_pool = nullptr;
std::size_t g_cap   = 0;

struct InputsInit {
    InputsInit() {
        g_cap = kInputs;
        g_pool = new Input[g_cap];
        for (std::size_t k = 0; k < g_cap; ++k) {
            g_pool[k].i = static_cast<int>(k * 31 + 7);
            g_pool[k].x = static_cast<unsigned>(k * 0x9E3779B9u + 0x1234u);
            g_pool[k].f = static_cast<double>(k) * 0.5 + 0.125;
            char buf[8];
            std::size_t len = 3 + (k % 5);
            for (std::size_t t = 0; t < len; ++t) {
                buf[t] = static_cast<char>('a' + ((k + t) % 26));
            }
            buf[len] = '\0';
            g_pool[k].s.assign(buf, len);
            g_pool[k].c = static_cast<char>('A' + (k % 26));
        }
    }
};

InputsInit g_inputs_init; // 触发 main 之前填充 g_pool

} // namespace

int main() {
    // 0) 准备: 输出到 stdout, 不重定向. 4 路径公平地承担终端翻屏 / 同步锁
    //    等"用户态 I/O"成本 —— 这正是用户感知到的延迟.
    // ios_base::sync_with_stdio(false) + cin.tie(nullptr) 是 cout 的"公平设置".
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Input*      A = g_pool;
    std::size_t N = g_cap;

    std::size_t em_fmt_ms, std_printf_ms, std_cout_ms, std_print_ms;

    constexpr int kOuter = 100;
    constexpr int kInner = 1145;
    using namespace std::chrono;

    // ---- 1) em::fprint<"...">(stdout, ...)  编译期格式串 ---------------------
    {
        auto start = steady_clock::now();
        std::size_t acc = 0; // 防止整个循环被优化
        for (int i = 0; i < kOuter; ++i) {
            for (int j = 0; j < kInner; ++j) {
                std::size_t k = static_cast<std::size_t>(i * kInner + j) % N;
                em::fprint<":{:d} {:#x} {:.3f} {:s} {:c}\n">(
                    stdout, A[k].i, A[k].x, A[k].f, A[k].s, A[k].c);
                acc += A[k].i;
            }
        }
        auto end = steady_clock::now();
        em_fmt_ms = static_cast<std::size_t>(duration_cast<milliseconds>(end - start).count());
        if (acc == 0) std::fprintf(stderr, "x"); // 永远不发生, 仅消耗 acc
    }

    // ---- 2) std::printf(...)  运行时格式串 ----------------------------------
    {
        auto start = steady_clock::now();
        std::size_t acc = 0;
        for (int i = 0; i < kOuter; ++i) {
            for (int j = 0; j < kInner; ++j) {
                std::size_t k = static_cast<std::size_t>(i * kInner + j) % N;
                std::printf(":%d %#x %.3f %s %c\n",
                            A[k].i, A[k].x, A[k].f, A[k].s.c_str(), A[k].c);
                acc += A[k].i;
            }
        }
        auto end = steady_clock::now();
        std_printf_ms = static_cast<std::size_t>(duration_cast<milliseconds>(end - start).count());
        if (acc == 0) std::fprintf(stderr, "x");
    }

    // ---- 3) std::cout << ...  流式 ------------------------------------------
    {
        // 公平对齐: 用 std::fixed + setprecision(3) 与 printf "%.3f" 等价;
        // 默认 general 精度 6 会把 1000.125 截成 "1000.12" 与另三个不一致.
        std::cout << std::fixed << std::setprecision(3);
        auto start = steady_clock::now();
        std::size_t acc = 0;
        for (int i = 0; i < kOuter; ++i) {
            for (int j = 0; j < kInner; ++j) {
                std::size_t k = static_cast<std::size_t>(i * kInner + j) % N;
                std::cout << ':' << A[k].i << ' '
                          << std::hex << std::showbase
                          << A[k].x << std::dec << std::noshowbase
                          << ' '
                          << A[k].f << ' '
                          << A[k].s << ' '
                          << A[k].c << '\n';
                acc += A[k].i;
            }
        }
        std::cout.flush();
        auto end = steady_clock::now();
        std_cout_ms = static_cast<std::size_t>(duration_cast<milliseconds>(end - start).count());
        if (acc == 0) std::fprintf(stderr, "x");
    }

    // ---- 4) std::println(...)  std::format 风格 -----------------------------
    {
        auto start = steady_clock::now();
        std::size_t acc = 0;
        for (int i = 0; i < kOuter; ++i) {
            for (int j = 0; j < kInner; ++j) {
                std::size_t k = static_cast<std::size_t>(i * kInner + j) % N;
                std::println(":{} {:#x} {:.3f} {} {}",
                             A[k].i, A[k].x, A[k].f, A[k].s, A[k].c);
                acc += A[k].i;
            }
        }
        auto end = steady_clock::now();
        std_print_ms = static_cast<std::size_t>(duration_cast<milliseconds>(end - start).count());
        if (acc == 0) std::fprintf(stderr, "x");
    }

    // ---- 报告 (走 stderr, 不影响 stdout 的 I/O 路径) ------------------------
    std::fprintf(stderr,
                 "[test_speed] 100 x 1145 = 114500 calls, 5 fields each, -> stdout\n"
                 "  em_fmt   : %6zu ms\n"
                 "  printf   : %6zu ms\n"
                 "  cout     : %6zu ms\n"
                 "  println  : %6zu ms\n"
                 "  ratio em_fmt / printf = %.2fx\n",
                 em_fmt_ms, std_printf_ms, std_cout_ms, std_print_ms,
                 static_cast<double>(em_fmt_ms) / static_cast<double>(std_printf_ms));
    return 0;
}
