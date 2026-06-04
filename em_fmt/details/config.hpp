#pragma once

// em_fmt has no runtime buffer strategy. Each write_argument overload uses
// a tiny stack-local `digits[]` (24 bytes for integers, 32 bytes for
// floating-point) and emits the result directly to the FILE* via fputc /
// small fwrite calls. Total stack use per fprint call is ≤ 64 bytes.
//
// All format-spec parsing is done at compile time. The runtime fprint
// path is a straight sequence of fwrite / fputc calls driven by a
// pre-computed `static constexpr std::array<arg_plan, …>` indexed by
// argument position. No spec field is examined at runtime; the
// `arg_plan` is the materialized result of compile-time parsing.
