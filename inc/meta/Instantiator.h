#include <utility>
#include <array>

namespace meta {

template<
    template<std::size_t> class UserTemplate, std::size_t Size, typename... Args
>
struct Instantiator {
    using return_t = decltype(UserTemplate<0>::execute(std::declval<Args>()...));
    using signature_t = return_t (*)(Args...);

    static return_t execute(Args... arguments, std::size_t index) {
        constexpr static auto table =
            makeCallTable(std::make_index_sequence<Size>());
        return table[index](std::forward<Args>(arguments)...);
    }

private:
    template<std::size_t... Indices>
    constexpr static std::array<signature_t, Size>
    makeCallTable(std::index_sequence<Indices...>) {
        return { UserTemplate<Indices>::execute... };
    }
};

}
