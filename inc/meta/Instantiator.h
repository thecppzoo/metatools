#ifndef ZOO_META_INSTANTIATOR
#define ZOO_META_INSTANTIATOR

#ifndef SIMPLIFY_PREPROCESSING
#include <utility>
#include <array>
#endif

namespace meta {

template<
    template<std::size_t> class UserTemplate,
    std::size_t Size,
    typename FunctionSignature
>
struct Instantiator;

template<
    template<std::size_t> class UserTemplate,
    std::size_t Size,
    typename Return,
    typename... Args
>
struct Instantiator<UserTemplate, Size, Return(Args...)> {
    using return_t = Return;
    using signature_t = return_t (*)(Args...);

    static return_t execute(Args... arguments, std::size_t index) {
        constexpr static auto jumpTable =
            makeJumpTable(std::make_index_sequence<Size>());
        return jumpTable[index](std::forward<Args>(arguments)...);
    }

private:
    template<std::size_t... Indices>
    constexpr static std::array<signature_t, Size>
    makeJumpTable(std::index_sequence<Indices...>) {
        return {{ UserTemplate<Indices>::execute... }};
    }
};

template<
    template<std::size_t> class UserTemplate,
    std::size_t Size,
    typename Signature
>
struct SwitchInstantiator;

template<
    template<std::size_t> class UserTemplate,
    std::size_t Size,
    typename Return,
    typename... Args
>
struct SwitchInstantiator<UserTemplate, Size, Return(Args...)> {
    static Return execute(Args... args, std::size_t index) {
        return doer<0>(std::forward<Args>(args)..., index);
    }

private:
    template<std::size_t Ndx>
    static std::enable_if_t<Ndx < Size - 1, Return>
    doer(Args... args, std::size_t index) {
        switch(index) {
            case Ndx:
                return UserTemplate<Ndx>::execute(std::forward<Args>(args)...);
            default:
                doer<Ndx + 1>(std::forward<Args>(args)..., index);
        }
    }

    template<std::size_t Ndx>
    static std::enable_if_t<Ndx == Size - 1, Return>
    doer(Args... args, std::size_t) {
        return UserTemplate<Ndx>::execute(std::forward<Args>(args)...);
    }
};

}

#endif

