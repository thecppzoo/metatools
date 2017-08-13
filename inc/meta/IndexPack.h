#pragma once

namespace meta {

template<typename T, T... indices> struct IndexPack {
    static constexpr auto arity = sizeof...(indices);
};

}
