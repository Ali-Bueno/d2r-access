#pragma once
#include <cstdlib>
#include <iostream>
#include <string_view>

inline int checks = 0;
inline void Check(bool value, std::string_view description) {
	++checks;
	if (!value) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
}
inline void Passed() { std::cout << checks << " checks passed\n"; }
