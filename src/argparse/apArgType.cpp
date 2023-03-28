/****************************************************************************
  FileName     [ apArgType.cpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define parser argument types ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "apArgType.h"

#include <iostream>

using namespace std;

namespace ArgParse {

namespace ArgTypeDescription {

std::string getTypeString(bool) { return "bool"; }

std::string getTypeString(int) { return "int"; }
std::string getTypeString(long) { return "long"; }
std::string getTypeString(long long) { return "long long"; }

std::string getTypeString(unsigned) { return "unsigned"; }
std::string getTypeString(unsigned long) { return "size_t"; }
std::string getTypeString(unsigned long long) { return "size_t"; }

std::string getTypeString(float) { return "float"; }
std::string getTypeString(double) { return "double"; }
std::string getTypeString(long double) { return "long double"; }

std::string getTypeString(std::string const& val) { return "string"; }

std::ostream& print(std::ostream& os, bool val) { return os << val; }
std::ostream& print(std::ostream& os, std::string const& val) { return os << val; }

bool parseFromString(bool& val, std::string const& token) {
    if (myStrNCmp("true", token, 1) == 0) {
        val = true;
        return true;
    } else if (myStrNCmp("false", token, 1) == 0) {
        val = false;
        return true;
    }
    return false;
}

bool parseFromString(std::string& val, std::string const& token) {
    val = token;
    return true;
}

}  // namespace ArgTypeDescription

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