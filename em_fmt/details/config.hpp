#pragma once
#define USE_STATIC_BUFFER true
constexpr auto USE_STACK_BUFFER = !USE_STATIC_BUFFER;
static_assert(!(USE_STATIC_BUFFER && USE_STACK_BUFFER), "Only can chose one buffer way");

constexpr auto STATIC_BUFFER_SIZE = 64;
constexpr auto STACK_BUFFER_SIZE  = 8;

constexpr auto FLOAT_FORMAT_BY_STD_TO_CHAR  = false;
constexpr auto FLOAT_FORMAT_BY_STD_SNPRINTF = !FLOAT_FORMAT_BY_STD_TO_CHAR;
static_assert(!(FLOAT_FORMAT_BY_STD_TO_CHAR && FLOAT_FORMAT_BY_STD_SNPRINTF), "Only can chose one format way");
