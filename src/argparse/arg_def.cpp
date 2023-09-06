/****************************************************************************
  PackageName  [ argparse ]
  Synopsis     [ Define concepts and templates for argparse::ArgType ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./arg_def.hpp"

#include <span>
#include <string>

#include "util/util.hpp"

namespace argparse {

template <>
std::string type_string(int const& /*unused*/) { return "int"; }
template <>
std::string type_string(long const& /*unused*/) { return "long"; }
template <>
std::string type_string(long long const& /*unused*/) { return "long long"; }

template <>
std::string type_string(unsigned const& /*unused*/) { return "unsigned"; }
template <>
std::string type_string(unsigned long const& /*unused*/) { return "size_t"; }
template <>
std::string type_string(unsigned long long const& /*unused*/) { return "size_t"; }

template <>
std::string type_string(float const& /*unused*/) { return "float"; }
template <>
std::string type_string(double const& /*unused*/) { return "double"; }
template <>
std::string type_string(long double const& /*unused*/) { return "long double"; }

template <>
std::string type_string(std::string const& /*unused*/) { return "string"; }
template <>
std::string type_string(bool const& /*unused*/) { return "bool"; }
template <>
std::string type_string(DummyArgType const& /*unused*/) { return "dummy"; }

template <>
bool parse_from_string(bool& val, std::string const& token) {
    using namespace std::string_literals;
    using dvlab::str::to_lower_string;
    if ("true"s.starts_with(to_lower_string(token))) {
        val = true;
        return true;
    } else if ("false"s.starts_with(to_lower_string(token))) {
        val = false;
        return true;
    }
    return false;
}

template <>
bool parse_from_string(std::string& val, std::string const& token) {
    val = token;
    return true;
}

template <>
bool parse_from_string(DummyArgType& val, std::string const& token) {
    return true;
}

}  // namespace argparse