# Converting runtime values to compile time values, "meta-switch"

## This document explains the work done in [`Instantiator.h`](https://github.com/thecppzoo/metatools/blob/master/inc/meta/Instantiator.h)

There are many use cases of decoding data from an input stream, where one gets a type switch field, something like the message type id in market data messages, or the opcode in a processor emulator.  Using that type switch, it is possible to jump into the appropriate function that will take care of the data.

Whenever implementing these things manually, one uses the core language feature of a `switch`.  The explanations will use the following example, inspired by the processing of financial market data, a subject matter I presented at [CPPCon last year](https://www.youtube.com/watch?v=z6fo90R8q5U), [code](https://github.com/thecppzoo/cppcon2016)

```c++
switch(messageId) {
    ...
    case INCREMENTAL: processIncremental(reinterpret_cast<Incremental *>(data)); break;
    ...
}
```

However, this looks wasteful in terms of programmer effort: the binding between `INCREMENTAL` and `processIncremental` should already be explicit, the programmer should not have to repeat themselves by writing the switch and the cases.  As I always criticize whenever the language incentivizes copying & pasting, this is dangerous, for example, the set of message types can change and then every single one of the switches needs updating.  There are other problems of copying and pasting that apply here, but I don't want to repat here the reasons why copying & pasting is so bad.

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
```

## Discriminating unions, `std::variant`

This soluiton I think is applicable to use cases regarding discriminating unions (that have a runtime type switch field) and also to compile-time switching as in some `std::variant` visitor patterns.

## Example

Now, consider this skeleton example:

```c++
enum MessageType: std::size_t {
    INCREMENTAL = 3,
    TRADE = 5,
    INVALID
};

template<std::size_t>
struct Message {};

using Incremental = Message<INCREMENTAL>;
using Trade = Message<TRADE>;

void processIncremental(const Incremental &);
void processTrade(const Trade &);

template<>
struct Message<INCREMENTAL> {
    static void process(const Incremental &data) { processIncremental(data); }
};

template<>
struct Message<TRADE> {
    static void process(const Trade &data) { processTrade(data); }
};

template<std::size_t MT, typename = void>
struct MessageProcessor_impl {
    static void execute(const void *data) {}
};

template<typename...> using void_t = void;

template<std::size_t MT>
struct MessageProcessor_impl<MT, void_t<decltype(Message<MT>::process)>> {
    static void execute(const void *data) {
        Message<MT>::process(*reinterpret_cast<const Message<MT> *>(data));
    }
};

template<std::size_t MT>
struct MessageProcessor: MessageProcessor_impl<MT> {};

void process(const void *data, int id) {
    meta::Instantiator<
        MessageProcessor, INVALID, const void *
    >::execute(data, id);
}
```

The [compiler explorer](https://gcc.godbolt.org/#z:OYLghAFBqd5QCxAYwPYBMCmBRdBLAF1QCcAaPECAKxAEZSBnVAV2OUxAHIBSAJgGY8AO2QAbZlgDU3fgGFmBPKMIBPGdm4AGAIJ9BI8VJmyAhsWIm1/DTq3ahJgLaYGABxPtJzgiekB2ACE7OwJMR1dRE1DjO0k4yVDwyOi5BgJ0EBAGPAAvTAB9AnVJMRMGBkkAVQZMYgAVMIiozFJJNIys3IKCSQBlLtaCFVdMB2cAOknJbWJgBmDrOzTiZmQegEkhNJMhRSiSfyCdeMlmbKFgSWJMAlYhQul%2BABFJLDEhkYhq2oak5uNNOpMpgAB6YZAKTAQdqZN6iABuJlExhmc3UEAAlJNxhiMTIjtoTmdhJdssAHLdrg8ZC9rpT7j0IAAqDEQVEMbF4/gE2LxbaKZBXG53B6g8GQtmzDlTMzAZjOXYMVowzp5B7CLAgjGHXknOJoLahEGuYhtHwCyQmBSoBImABGokwjyeur1J0cJgA1phTKJRHV7Y7oelMh7vfkNaD8jUAI7MUbsYz9PLo3H410nOl3W0OzDcACsAUjIILT2DHQAZiQAO5mdAoqXo2Xy0YEBhYyZcgknbh%2BF22HQmvCI0IgV2JJopWQq7JqgjYySbfDseaLY7xA1pUEms1RPCClVmCxWad4clRVjdVrJvNrwnxMM%2BpH%2BwNQlXF6OYOMJ2%2ByJf7lxsXUbVe27N0hXpQ4qhqepGmSX9/xXIEQDFCFQgXXsXW5V1MOCPt0wHXR8MI0Z5UkABZFwGBMYBMDqYYODNDpZ26HV1zidYADlZAAJWwcjsE4uptAAGWdSR%2BFIV06h47QnmwcT8yk9jF04gA1UT1n7IisJ5HQJ3g4wZy6Qp1CWAgVjWCiqJop1QNw7DCOJC5FxEa4FR8URxMo8pbOMLjeP4wThJEszHO0ZzLjqCwjGeazfNo4wZLkjRFnCux4VQPB0EkE1UBXBhNmQdzWyRCBNw2NywlKry%2BAANi7DKspyvKCuikwsHK1BDUkdqjF4BqCN0fS4P%2BOQzJ0ZZVh6HzqMSuQAr4gShNE4p7JU/l90kTLsty4h8qorqeqKkrdiRaQBvQKITBAwI9oO8oTuqs7RAgK6fC7fxtIcvTtAMsbZAm7Qpqs2a/LkZL5LWwJXU2wUdpa/aCqOtJepiuzLuu26Anutr0berH8S%2BvDdOCEa/inYy5woupBgYsY7LihGgZBmabNogAFJGqJICMkjY%2B84jh7bmskVCJQqkXdqZd6bp1YidKGkJRqnD5RicTAgOsU5zkuBHqSZ5qlfJydfyp1jyLqFmLOm%2BK5swLmHqYYg%2BYiYxLdafWijkOE1YgMH5tkS3kNaqiuVShT1sF3cLQRsWwTQqFJbjmWsYF8C7fBoOresTJQ/KZlrmEUJiBNG58mQMpvdkSWA9/YPtaZdFZbTcKewVn6yb%2BlWzZDVULZzmxgZt0H2Yd7nyhIEBM85ifndd5E5AbyOYfw9KdDj/OGBRnoU9l1pi8kbLsddbwTEyTZtl2PB9mIGIVJOOvHYKkhWi4jSRK01pk9FpuVOQ8WoQCY%2BAPugRqtg%2BycAxKQUQXB8ycFIEILgmgEGoC4IDXgQRMFtBYGwDG/BaAIIIMgqBpAIBIBYAQVwChyCUDQOEJQtQ6CkEwPgIgxA6DQNgZweBiCSGkDQTwAQtBJDVkIAgSQIIAAcdUAC0dUAAsJRIguXzOMTQ6iiEkNxKQT0IB8xSPGAATk0EY/gfg/AEKMTIhR%2BZJLcIUQgpBnAUECK4AghgIBNCkGIS40hcBYBIHoa4RhZAKBdQYY6DhpQLj5k0N4isSgS6eIgHafh9CPIAHkhCiBUPw/A1w1jDhcPw4umBuGuIsngRw2iBGuEUN1TxnBJCyMybwFp7QaTID4FggItAlGyIAOrPg8bg9gzDay7EyfUzYVYuCEK4XApx/DBHSLkYo5ROxLhqI0ZoSQEBcCEAOHoegkhZCoEibUC6BCMRaL8TovRCjaDqJMXVXgA0pGaFoPwfMCijEwK4I4vhfi3GcA8V4nx2jFmcF4MskFgi7koJ0fCWo2RuogAUUAA%3D) shows that the function `process` is implemented as simply jumping into a table using `id` as the index.  This is equivalent to how non trivial `switch` are treated.

I would say the only specific work needed for using `Instantiator` is the adaptor, which in this example, takes care of "doing nothing" if the specialization of `Message` does not have a `process` member, 11 lines of as straightforward code as it gets in metaprogramming.

