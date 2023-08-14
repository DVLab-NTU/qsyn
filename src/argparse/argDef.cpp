/****************************************************************************
  FileName     [ argDef.hpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define ArgParse::ArgType template implementation ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./argDef.hpp"

#include <span>
#include <string>

#include "util/util.hpp"

namespace ArgParse {

template <>
std::string typeString(int const&) { return "int"; }
template <>
std::string typeString(long const&) { return "long"; }
template <>
std::string typeString(long long const&) { return "long long"; }

template <>
std::string typeString(unsigned const&) { return "unsigned"; }
template <>
std::string typeString(unsigned long const&) { return "size_t"; }
template <>
std::string typeString(unsigned long long const&) { return "size_t"; }

template <>
std::string typeString(float const&) { return "float"; }
template <>
std::string typeString(double const&) { return "double"; }
template <>
std::string typeString(long double const&) { return "long double"; }

template <>
std::string typeString(std::string const& val) { return "string"; }
template <>
std::string typeString(bool const&) { return "bool"; }
template <>
std::string typeString(DummyArgType const&) { return "dummy"; }

template <>
bool parseFromString(bool& val, std::string const& token) {
    using namespace std::string_literals;
    if ("true"s.starts_with(toLowerString(token))) {
        val = true;
        return true;
    } else if ("false"s.starts_with(toLowerString(token))) {
        val = false;
        return true;
    }
    return false;
}

template <>
bool parseFromString(std::string& val, std::string const& token) {
    val = token;
    return true;
}

template <>
bool parseFromString(DummyArgType& val, std::string const& token) {
    return true;
}

}  // namespace ArgParse