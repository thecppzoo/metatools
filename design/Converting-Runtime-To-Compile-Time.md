# Converting runtime values to compile time values, "meta-switch"

There are many use cases of decoding data from an input stream, where one gets a type switch field, something like the message type id in market data messages, or the opcode in a processor emulator.  Using that type switch, it is possible to jump into the appropriate function that will take care of the data.

Whenever implementing these things manually, one uses the core language feature of a `switch`.  The explanations will use the following example:

```c++
switch(messageId) {
    ...
    case INCREMENTAL: processIncremental(reinterpret_cast<Incremental *>(data)); break;
    ...
}
```

However, this looks wasteful in terms of programmer effort: the binding between INCREMENTAL and processIncremental should already be explicit, the programmer should not have to repeat themselves by writing the switch and the cases.  As I always criticize whenever the language incentivizes copying & pasting, this is dangerous, for example, the message set can change and then every single one of the switches needs updating.  There are other problems of copying and pasting that apply here, but I don't want to repat here the reasons why copying & pasting is so bad.

Since the binding between `INCREMENTAL` and `processIncremental` can be made explicit in many different ways, how are we going to program that whenever the `messageId` is `INCREMENTAL` then `processIncremental` should be called?

It is reasonable to model the messages as a template of an integral value.  Without losing generality, let us say it is a `std::size_t`:

```c++
template<std::size_t MessageId> struct Message;
```

`Message` can be specialized for the different types of messages:

```c++
template<> struct Message<INCREMENTAL> {
    // ...
    static void process(Message<INCREMENTAL> &incremental);
    // ...
};
```

Now, let us create an adaptor template in which a class-member function `execute` will invoke the `process`, the reason will be explained soon:

```c++
template<std::size_t Id> struct Adaptor {
    static void execute(void *data) { Message<Id>::process(*reinterpret_cast<Message<Id> *>(data)); }
};
```

Now it is possible to create a table of functions that invoke the adaptor, like this:

```c++
std::array<void (*)(void *), MessageTypeCount> table = {
    Adaptor<0>::execute, Adaptor<1>::execute, ..., Adaptor<MessageTypeCount>::execute
};
```
Writing this table would be putting in the same effort as writing a `switch`, but this can be done automatically, unlike the `switch`, with a template parameter pack of indices captured into a `std::index_sequence`:

```c++
template<typename Signature, std::size_t... Indices>
std::array<Signature, sizeof...(Indices)> make_table(std::index_sequence<Indices...>) {
    return { Adaptor<Indices>::execute... };
}
```

Now we can convert the runtime value of the message id into the correct invocation:

```c++
void invokes(std::size_t messageId, void *messageData) {
    table[messageId](messageData);
}
```

Generalizing, and taking into account all the `constexpr`, `static`, template templates, etc., we arrive at something like this:

```c++
#include <utility>
#include <array>

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
```

Which is implemented in this repository as `Instantiator`.
