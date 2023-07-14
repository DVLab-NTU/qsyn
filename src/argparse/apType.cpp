/****************************************************************************
  FileName     [ apArgType.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define parser argument types ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <iostream>

#include "argparse.h"

using namespace std;

namespace ArgParse {

template <>
std::string typeString(int) { return "int"; }
template <>
std::string typeString(long) { return "long"; }
template <>
std::string typeString(long long) { return "long long"; }

template <>
std::string typeString(unsigned) { return "unsigned"; }
template <>
std::string typeString(unsigned long) { return "size_t"; }
template <>
std::string typeString(unsigned long long) { return "size_t"; }

template <>
std::string typeString(float) { return "float"; }
template <>
std::string typeString(double) { return "double"; }
template <>
std::string typeString(long double) { return "long double"; }

std::string typeString(std::string const& val) { return "string"; }
std::string typeString(bool) { return "bool"; }
std::string typeString(DummyArgumentType) { return "dummy"; }

bool parseFromString(bool& val, TokensView tokens) {
    if (myStrNCmp("true", tokens[0].token, 1) == 0) {
        val = true;
        return true;
    } else if (myStrNCmp("false", tokens[0].token, 1) == 0) {
        val = false;
        return true;
    }
    return false;
}

bool parseFromString(std::string& val, TokensView tokens) {
    val = tokens[0].token;
    return true;
}

bool parseFromString(DummyArgumentType& val, TokensView tokens) {
    return true;
}

/**
 * @brief generate a callback that sets the argument to true.
 *        This function also set the default value to false.
 *
 * @param arg
 * @return ArgParse::ActionCallbackType
 */
ActionCallbackType storeTrue(ArgType<bool>& arg) {
    arg.defaultValue(false);
    arg.constValue(true);

    return [&arg]() -> bool {
        arg.setValueToConst();
        return true;
    };
}

/**
 * @brief generate a callback that sets the argument to false.
 *        This function also set the default value to true.
 *
 * @param arg
 * @return ArgParse::ActionCallbackType
 */
ActionCallbackType storeFalse(ArgType<bool>& arg) {
    arg.defaultValue(true);
    arg.constValue(false);
    return [&arg]() -> bool {
        arg.setValueToConst();
        return true;
    };
}

}  // namespace ArgParse