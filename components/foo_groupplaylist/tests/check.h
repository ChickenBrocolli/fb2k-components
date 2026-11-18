#pragma once

#include <cstdio>

// A failed check prints its expression and line, and the test program exits with a nonzero code.
inline int& FailureCount() {
	static int count = 0;
	return count;
}

inline void Check(bool ok, const char* expression, const char* file, int line) {
	if (ok) return;
	++FailureCount();
	std::printf("FAILED %s line %d: %s\n", file, line, expression);
}

#define CHECK(expression) Check((expression), #expression, __FILE__, __LINE__)
