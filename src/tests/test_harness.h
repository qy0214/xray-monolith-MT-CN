#pragma once
// Zero-dependency micro test harness. CTest friendly: a non-zero exit code means
// failure, and only failures are printed (plus one summary line), so CI output stays
// readable. No external framework is vendored in this repository.
#include <cstdio>

namespace xrtest
{
	inline int& failures()
	{
		static int value = 0;
		return value;
	}

	inline int& checks()
	{
		static int value = 0;
		return value;
	}

	inline void check(bool ok, const char* expr, const char* file, int line)
	{
		++checks();
		if (ok)
			return;
		++failures();
		std::printf("[FAIL] %s:%d  %s\n", file, line, expr);
	}
}

#define CHECK(expr) ::xrtest::check((expr), #expr, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, eps) ::xrtest::check(((a) - (b) <= (eps)) && ((b) - (a) <= (eps)), #a " ~= " #b, __FILE__, __LINE__)
