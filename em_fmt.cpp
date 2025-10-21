#include "em_fmt.hpp"
#include <cstdio>

using namespace em;

int main() {
    fprint<"123456789\n">(stdout);
    fprint<"6{}8{}">(stdout, 7, 9);
}