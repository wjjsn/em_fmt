// fprint_instantiations.cpp: this TU instantiates fprint with many type
// combinations so the static analyzer (-fstack-usage) sees the actual frame
// sizes of em::fprint and em::write_argument.
#include "em_fmt.hpp"
#include <cstdio>
#include <cstdint>
#include <string>
#include <string_view>

extern "C" {

#define DEF(name, fmt, ...)                                                  \
    __attribute__((noinline))                                                \
    int name(FILE *f) {                                                      \
        return em::fprint<fmt>(f, __VA_ARGS__);                              \
    }

DEF(fi_i,    "{} {}", 42, 7)
DEF(fi_x,    "{:#x} {:X}", 0xCAFEu, 0xBEEFu)
DEF(fi_d,    "{:d} {:+d} {: d}", 7, 7, 7)
DEF(fi_w,    "{:5} {:<5} {:>5} {:*^5}", 42, 42, 42, 42)
DEF(fi_f,    "{:.2f} {:+.3e} {:a}", 3.14, 0.001, 1.5)
DEF(fi_F,    "{:.3F} {:A}", 1.5, 1.5)
DEF(fi_s,    "{:s} {:>5.2s}", "hello", "hi")
DEF(fi_c,    "{:c} {:c}", 'A', 65)
DEF(fi_b,    "{:s} {}", true, 0)
DEF(fi_p,    "{:p} {:#P}", (void *)0x1234, (void *)0x1234)
DEF(fi_n,    "{:p}", (void *)nullptr)
DEF(fi_thou, "{:,d}", 1234)
DEF(fi_idx,  "{} {}", 1, 2)
DEF(fi_sgn,  "{:+d} {: d}", 7, 3)
DEF(fi_pad,  "{:*<+8d}", 42)
DEF(fi_8,    "{:d} {:d} {:d}", 1, 2, 3)

}  // extern "C"
