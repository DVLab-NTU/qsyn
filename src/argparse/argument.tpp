/****************************************************************************
  FileName     [ argument.tpp ]
  PackageName  [ argparser ]
  Synopsis     [ Define ArgParse::Argument template implementation ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include "./argparse.hpp"

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
T Argument::get() const {
    if constexpr (IsContainerType<T>) {
        using V = typename std::remove_cv<typename T::value_type>::type;
        if (auto ptr = dynamic_cast<Model<ArgType<V>>*>(_pimpl.get())) {
            return ptr->inner.template get<T>();
        }
    } else {
        if (auto ptr = dynamic_cast<Model<ArgType<T>>*>(_pimpl.get())) {
            return ptr->inner.template get<T>();
        }
    }

    std::cerr << "[ArgParse] Error: cannot cast argument \""
              << getName() << "\" to target type!!\n";
    throw std::bad_cast{};
}

}  // namespace ArgParse
