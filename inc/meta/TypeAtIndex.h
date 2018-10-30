#ifndef ZOO_META_TYPE_AT_INDEX
#define ZOO_META_TYPE_AT_INDEX

#include <meta/Pack.h>
#include <cstddef>

namespace meta {
namespace detail {

template<std::size_t Remaining, typename Head, typename... Tail>
struct TypeAtIndexByRemaining {
    using type = typename TypeAtIndexByRemaining<Remaining - 1, Tail...>::type;
};

template<typename Head, typename... Tail>
struct TypeAtIndexByRemaining<0, Head, Tail...> {
    using type = Head;
};

}

template<std::size_t Index, typename... Ts>
struct TypeAtIndex {
    using type = typename detail::TypeAtIndexByRemaining<Index, Ts...>::type;
};

template<std::size_t Index, typename... Ts>
struct TypeAtIndex<Index, Pack<Ts...>> {
    using type = typename TypeAtIndex<Index, Ts...>::type;
};

template<std::size_t Index, typename... Ts>
using TypeAtIndex_t = typename TypeAtIndex<Index, Ts...>::type;

}

#endif
