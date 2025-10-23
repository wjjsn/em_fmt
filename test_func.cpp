#include "em_fmt.hpp"
// #include <vector>
#include <cstdint>

int main() {
    em::fprint<"int={}, long={}, double={}, float={}, char={}, cstr={}, str={}, strv={}, bool={}, ptr={}, nullptr={}, "
               " uint64={}, int8={}, uint16={}, size_t={}, hex={}, sci={}\n">(
        stdout,
        42,
        1234567890L,
        3.1415926,
        2.71828f,
        'A',
        "C-string",
        std::string("std::string"),
        std::string_view("std::string_view"),
        true,
        (void *)0x1234ABCD,
        nullptr, 
        // std::vector<int>{ 1, 2, 3, 4 },//暂未支持
        static_cast<uint64_t>(9876543210ULL),
        static_cast<int8_t>(-5),
        static_cast<uint16_t>(65535),
        static_cast<size_t>(123456),
        0xBEEF,
        0.00123);

    return 0;
}