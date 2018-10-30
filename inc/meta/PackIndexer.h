#ifndef ZOO_META_PACK_INSTANTIATOR
#define ZOO_META_PACK_INSTANTIATOR

#include <meta/TypeAtIndex.h>

namespace meta {

template<template<typename> class Executor, typename... TypeArray>
struct PackIndexer {
    template<std::size_t Index>
    using Internal = Executor<TypeAtIndex_t<Index, TypeArray...>>;
};

}

#endif
