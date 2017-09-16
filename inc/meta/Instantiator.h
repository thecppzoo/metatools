#include <utility>
#include <array>

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
        return { UserTemplate<Indices>::execute... };
    }
};

}
