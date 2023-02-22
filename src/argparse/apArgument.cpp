/****************************************************************************
  FileName     [ argument.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define argument interface for ArgumentParser ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "apArgument.h"

#include <iomanip>
#include <iostream>

using namespace std;

namespace ArgParse {

void Argument::printArgCastErrorMsg() const {
    std::cout << "[ArgParse] Error: cannot cast argument \""
              << getName() << "\" to target type!!" << std::endl;
}

void Argument::printStatus() const {
    cout << "  " << left << setw(8) << getName() << "   = ";
    if (isParsed()) {
        cout << *this;
    } else if (hasDefaultValue()) {
        cout << *this << " (default)";
    } else {
        cout << "(unparsed)";
    }
    cout << endl;
}

}  // namespace ArgParse