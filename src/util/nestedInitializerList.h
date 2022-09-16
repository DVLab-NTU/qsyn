/****************************************************************************
  FileName     [ nestedInitializerList.h ]
  PackageName  [ util ]
  Synopsis     [ Definition of a nested initializer list ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ 2022 9 ]
****************************************************************************/
#ifndef NESTED_INITIALIZER_LIST_H
#define NESTED_INITIALIZER_LIST_H

#include <cstdlib>

namespace detail {
    template <class T, std::size_t I>
    struct nested_initializer_list
    {
        using type = std::initializer_list<typename nested_initializer_list<T, I - 1>::type>;
    };

    template <class T>
    struct nested_initializer_list<T, 0>
    {
        using type = T;
    };
}

template <class T, std::size_t I>
using NestedInitializerList = typename detail::nested_initializer_list<T, I>::type;

#endif // NESTED_INITIALIZER_LIST_H