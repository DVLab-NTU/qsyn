/****************************************************************************
  FileName     [ apArgument.h ]
  PackageName  [ argparser ]
  Synopsis     [ Define ArgParse::Argument template implementation ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#ifndef AP_ARGUMENT_TPP
#define AP_ARGUMENT_TPP

#include "argparse.h"

namespace ArgParse {
/**
 * @brief Access the data stored in the argument.
 *        This function only works when the target type T is
 *        the same as the stored type; otherwise, this function
 *        throws an error.
 *
 * @tparam T the stored data type
 * @return T const&
 */
template <typename T>
requires ValidArgumentType<T>
T const& Argument::get() const {
    if (auto ptr = dynamic_cast<Model<ArgType<T>>*>(_pimpl.get())) {
        return ptr->inner;
    }

    std::cerr << "[ArgParse] Error: cannot cast argument \""
              << getName() << "\" to target type!!\n";
    throw std::bad_any_cast{};
}

}

#endif // AP_ARGUMENT_TPP