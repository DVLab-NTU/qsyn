/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Include std::cin, std::cout, std::cerr as minimally as possible ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifdef _LIBCPP_VERSION
// libc++ does not support externing cin, cout, cerr
#include <iostream>
#else
#include <iosfwd>

namespace std {
extern istream cin;
extern ostream cout;
extern ostream cerr;
}  // namespace std
#endif
