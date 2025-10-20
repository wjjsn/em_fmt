#include "em_fmt.hpp"

using namespace em;

int main()
{
	fprint<"6{}8{}">(stdout, 7, 9);
}