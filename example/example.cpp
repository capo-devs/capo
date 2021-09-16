#include <iostream>
#include <capo/capo.hpp>
#include <ktl/str_format.hpp>

int main() { std::cout << ktl::format("Capo v{}", capo::version_v) << '\n'; }
