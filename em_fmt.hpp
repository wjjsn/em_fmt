#pragma once
#include <array>
#include <print>
#include <stdio.h>
#include <charconv>

namespace em
{
	template <size_t N>
	struct fixed_string
	{
		std::array<char, N> data_;
		unsigned int need_arg_num	  = 0;
		unsigned int need_analyze_num = 0;
		consteval fixed_string(const char (&str)[N])
		{
			std::copy_n(str, N, data_.begin());

			/*计算需要的参数数量*/
			for (size_t i = 0; i < data_.size() - 1; ++i)
			{
				if (data_.at(i) == '{' && data_.at(i + 1) == '}')
				{
					++need_arg_num;
					++i; // skip next
				}
			}
		}
	};

	struct arg_analyze_result
	{
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

	template <fixed_string S>
	struct StringLiteral
	{
		static constexpr std::array<unsigned int, S.need_arg_num> arg_attribute{};
		consteval StringLiteral()
		{
			if constexpr (S.need_arg_num == 0) {}
			else
			{
			}
		}
	};

	template <fixed_string format, typename... T>
	int fprint(FILE *stream, T... args)
	{
		using StrLit			  = StringLiteral<format>;
		constexpr int acc_arg_num = sizeof...(args);
		std::array<char, 8> buf;
		static_assert(StrLit::arg_attribute.size() == acc_arg_num, "Error at args number");
		return 0;
	}
} // namespace em