#pragma once

#include "meta/IndexPack.h"

namespace meta {

template<unsigned long count, unsigned long... indices>
struct Indices_impl {
    using type = typename Indices_impl<count - 1, count, indices...>::type;
};

template<unsigned long... indices>
struct Indices_impl<0, indices...> {
    using type = IndexPack<unsigned long, 0, indices...>;
};

template<unsigned long count> using Indices = typename Indices_impl<unsigned long, count>::type;

}
