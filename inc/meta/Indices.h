namespace meta {

template<unsigned long count, unsigned long... indices>
struct Indices {
    using type = typename Indices<count - 1, count, indices...>::type;
};

template<unsigned long... indices>
struct Indices<0, indices...> {
    using type = IndexPack<unsigned long, 0, indices...>;
};

}
