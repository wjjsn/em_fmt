#pragma once
#include <array>
#include <charconv>
#include <cstddef>
#include <print>
#include <stdio.h>

namespace em {
template <size_t N> struct fixed_string {
  std::array<char, N> data_;
  unsigned int need_arg_num = 0;
  unsigned int need_analyze_num = 0;

  consteval bool check() {
    for (size_t i = 0; i < data_.size(); ++i) {
      if (data_[i] == '{') {
        if (data_[i + 1] == '{') {
          ++need_analyze_num;
          ++i; // skip next
          continue;
        } else {
          ++i;
          for (size_t j = i; j < data_.size(); ++j, ++i) {
            if (data_[j] == '}') {
              ++need_arg_num;
              ++need_analyze_num;
              break;
            } else if (j + 1 == data_.size()) {
              return false;
            }
          }
        }
      } else if (data_[i] == '}') {
        if (data_[i + 1] == '}') {
          ++need_analyze_num;
          ++i; // skip next
          continue;
        } else {
          return false;
          // static_assert([]
          // 				{ return false; }(), "The
          // numbers of '{' or '}' Error");
        }
      }
    }
    need_analyze_num == 0 ?: need_analyze_num++;
    return true;
  }
  consteval fixed_string(const char (&str)[N]) {
    std::copy_n(str, N, data_.begin());

    /*计算需要的参数数量*/
    /*也许应该试试返回一个need_arg_num，need_analyze_num，bool的元组或者其他办法*/
    if constexpr (check() /*这里不能用，因为编译器不认为这个是编译期常量*/) {
    }
  }
};

struct arg_analyze_result {
  /*写n字节*/
  unsigned int write_bytes;
  /*跳过n字节*/
  unsigned int skip_bytes;
  /*是否有变量要输出*/
  bool have_arg;
  /*如果有并且是整数，进制是？*/
  unsigned int int_base;
  /*如果有并且是浮点数，格式是？*/
  std::chars_format float_format;
};

template <fixed_string S> struct StringLiteral {
  static consteval std::array<arg_analyze_result, S.need_analyze_num>
  make_attributes() {
    std::array<arg_analyze_result, S.need_analyze_num> arr{};
    for (std::size_t i = 0; i < S.need_analyze_num; ++i) {
    }
    return arr;
  }

  static constexpr std::array<arg_analyze_result, S.need_analyze_num>
      arg_attribute = make_attributes();
};

template <fixed_string format, typename... T>
int fprint(FILE *stream, T... args) {
  using StrLit = StringLiteral<format>;
  constexpr int acc_arg_num = sizeof...(args);
  std::array<char, 8> buf;
  static_assert(format.need_arg_num == acc_arg_num, "Error at args number");

  if constexpr (format.need_analyze_num == 0) {
    fwrite(format.data_.data(), sizeof(char), format.data_.size() - 1, stream);
  }
  return 0;
}
} // namespace em