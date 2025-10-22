#include "em_fmt.hpp"
#include <cstddef>
#include <cstdio>
#include <chrono>
#include <iostream>
#include <ostream>
#include <print>
using namespace em;

int main() {
    std::size_t em_fmt, std_printf, std_cout, std_print;

    using namespace std::chrono;
    {
        auto start = steady_clock::now();
        for (int i = 0; i < 100; ++i) {
            for (int j = 0; j < 1145; ++j) {
                em::fprint<"em_fmt out :{} {}\n">(stdout, i, j);
            }
        }
        auto end   = steady_clock::now();
        auto dt_us = duration_cast<milliseconds>(end - start);
        em_fmt     = dt_us.count();
    }

    {
        auto start = steady_clock::now();
        for (int i = 0; i < 100; ++i) {
            for (int j = 0; j < 1145; ++j) {
                printf("std_printf :%d %d\n", i, j);
            }
        }
        auto end   = steady_clock::now();
        auto dt_us = duration_cast<milliseconds>(end - start);
        std_printf = dt_us.count();
    }
    {
        auto start = steady_clock::now();
        for (int i = 0; i < 100; ++i) {
            for (int j = 0; j < 1145; ++j) {
                std::cout << "std_cout   :" << i << " " << j << std::endl;
            }
        }
        auto end   = steady_clock::now();
        auto dt_us = duration_cast<milliseconds>(end - start);
        std_cout   = dt_us.count();
    }
    {
        auto start = steady_clock::now();
        for (int i = 0; i < 100; ++i) {
            for (int j = 0; j < 1145; ++j) {
                std::println("std_print  :{} {}", i, j);
            }
        }
        auto end   = steady_clock::now();
        auto dt_us = duration_cast<milliseconds>(end - start);
        std_print  = dt_us.count();
    }
    em::fprint<"em_fmt:{}ms\nprintf:{}ms\ncout:{}ms\nstd_print:{}ms\n">(stdout, em_fmt, std_printf, std_cout, std_print);
}
