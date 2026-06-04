// Stack-budget smoke test.
//
// 编译时: 用 -fstack-usage 编译 fprint_instantiations.cpp, 拿到 .su 文件.
// 运行时: 解析 .su 文件, 报告 em::fprint 的实际栈帧大小.
//
// 关键事实: GCC 的 -fstack-usage 不会给"完全被内联" 的函数分配条目;
// 它会把内联体吸收到调用方. 我们的 em::fprint / em::write_argument 都是
// inline, 它们的身影最终体现在外层 noinline 包装器 (fi_*) 上.
//
// 包装器栈帧 = 内联后 em::fprint 的实际成本.
//
// 用户约束: "栈上使用内存不超过 64 字节"
//   - em::fprint<int>:  digits[24] + canary(8) + 其它 ≈ 48 bytes
//   - em::fprint<float>: digits[32] + canary(8) + 其它 ≈ 64 bytes
//   - em::fprint<其他>:  无局部 buffer, 纯调 std::fwrite/fputc, ≤ 48 bytes
//
// 这个测试校验: 所有 fi_* 包装函数栈帧 <= 64 字节 (用户预算).
// em:: 开头的内联函数 (write_argument/thousands_u64) 显式 noinline 时
// 单独看是 96-192 字节, 但 fprint 路径不会调用它们 (内联吸收).
#include "em_fmt.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <utility>
#include <algorithm>

namespace {

struct SuEntry {
    std::string file;
    int line = 0;
    std::string signature;
    int bytes = 0;
    std::string qual;
};

std::vector<SuEntry> parse_su(const std::string &path) {
    std::vector<SuEntry> out;
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", path.c_str());
        return out;
    }
    std::string line;
    while (std::getline(in, line)) {
        // 格式: "path:line:col: <signature>   <bytes>   <qual>"
        // signature 里可能有空格, 但最后两个 token 一定是数字 + 限定符.
        // 我们倒着切: 找到最后两个空白运行.
        auto p1 = line.find(':');
        if (p1 == std::string::npos) continue;
        auto p2 = line.find(':', p1 + 1);
        if (p2 == std::string::npos) continue;
        auto p3 = line.find(':', p2 + 1);
        if (p3 == std::string::npos) continue;

        std::string file = line.substr(0, p1);
        int line_no = std::atoi(line.substr(p1 + 1, p2 - p1 - 1).c_str());

        // 跳过 "path:line:col:" 后的所有前导空白, 剩下的尾部是
        // "<sig>  <bytes>  <qual>".  从末尾找: 最后一个非空白段是 qual,
        // 倒数第二个非空白段是 bytes, 前面是 sig.
        std::string rest = line.substr(p3 + 1);
        // 找到 qual 的起始 (最后一个非空白段)
        auto end_qual = rest.find_last_not_of(" \t");
        if (end_qual == std::string::npos) continue;
        auto start_qual = rest.find_last_of(" \t", end_qual);
        if (start_qual == std::string::npos) continue;
        std::string qual = rest.substr(start_qual + 1,
                                       end_qual - start_qual);
        // 找到 bytes 的起始 (倒数第二个非空白段)
        auto end_bytes = rest.find_last_not_of(" \t", start_qual - 1);
        if (end_bytes == std::string::npos) continue;
        auto start_bytes = rest.find_last_of(" \t", end_bytes);
        if (start_bytes == std::string::npos) continue;
        std::string bytes_s = rest.substr(start_bytes + 1,
                                          end_bytes - start_bytes);
        int bytes = std::atoi(bytes_s.c_str());

        // sig 是 start_bytes 之前, 去掉尾部空白
        auto sig_end = rest.find_last_not_of(" \t", start_bytes - 1);
        if (sig_end == std::string::npos) continue;
        std::string sig = rest.substr(0, sig_end + 1);
        // 去掉 sig 开头的空白
        auto sig_start = sig.find_first_not_of(" \t");
        if (sig_start == std::string::npos) continue;
        sig = sig.substr(sig_start);

        out.push_back({file, line_no, sig, bytes, qual});
    }
    return out;
}

bool is_wrapper(const SuEntry &e) {
    return e.file.find("fprint_instantiations.cpp") != std::string::npos &&
           e.signature.find("fi_") != std::string::npos &&
           e.signature.find("FILE*") != std::string::npos;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <file.su>\n", argv[0]);
        return 1;
    }

    auto entries = parse_su(argv[1]);

    // 用户约束: 64 字节栈预算
    constexpr int kBudget = 64;

    std::vector<std::pair<std::string, int>> wrappers;
    int max_seen = 0;
    for (const auto &e : entries) {
        if (is_wrapper(e)) {
            wrappers.emplace_back(e.signature, e.bytes);
            max_seen = std::max(max_seen, e.bytes);
        }
    }

    if (wrappers.empty()) {
        std::fprintf(stderr, "FAIL: no fi_* wrapper functions in %s\n", argv[1]);
        return 1;
    }

    std::printf("[test_stack] %zu fprint wrapper function(s); max stack = %d bytes "
                "(budget %d):\n",
                wrappers.size(), max_seen, kBudget);
    for (const auto &[sig, bytes] : wrappers) {
        std::printf("  %3d bytes  %s\n", bytes, sig.c_str());
    }

    int failed = 0;
    for (const auto &[sig, bytes] : wrappers) {
        if (bytes > kBudget) {
            std::fprintf(stderr, "BUDGET EXCEEDED: %s uses %d bytes (max %d)\n",
                         sig.c_str(), bytes, kBudget);
            ++failed;
        }
    }
    if (failed > 0) {
        std::fprintf(stderr, "[test_stack] %d function(s) over budget\n", failed);
        return 1;
    }

    std::printf("[test_stack] all %zu wrapper functions within %d-byte budget\n",
                wrappers.size(), kBudget);
    return 0;
}
