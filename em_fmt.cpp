#include "em_fmt.hpp"
#include <iostream>

/*这个仅用于测试*/
template <size_t N> struct fixed_string {
  std::array<char, N> data_;
  unsigned int need_arg_num = 0;
  unsigned int need_analyze_num = 0;
  fixed_string(const char (&str)[N]) {
    std::copy_n(str, N, data_.begin());

    /*计算需要的参数数量*/
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
            }
          }
        }
      } else if (data_[i] == '}') {
        if (data_[i + 1] == '}') {
          ++need_analyze_num;
          ++i; // skip next
          continue;
        } else {
          std::println("                        error");
        }
      }
    }
    need_analyze_num == 0 ?: need_analyze_num++;
    std::println("\"{}\"\nneed_analyze_num:{}\nneed_arg_num{}", str,
                 need_analyze_num, need_arg_num);
  }
};
int main()
{
  // fixed_string a{"6{}8{}}"};
  em::fprint<"6{}8{}">(stdout, 7, 9);
  // fprint<"hello world\nn">(stdout);
}