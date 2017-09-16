# Converting runtime values to compile time values, "meta-switch", "compile-time-built jump table"

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
        return table[index](std::forward<Args>(arguments)...);
    }

private:
    template<std::size_t... Indices>
    constexpr static std::array<signature_t, Size>
    makeJumpTable(std::index_sequence<Indices...>) {
        return { UserTemplate<Indices>::execute... };
    }
};

}
```

## Discriminating unions, `std::variant`

This solution I think is applicable to use cases regarding discriminating unions (that have a runtime type switch field) and also to compile-time switching as in some `std::variant` visitor patterns.

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

# Related work

One example of the emerging superiority of Clang over GCC may be that it is possible to implement this meta-switch using the `switch` feature, more precisely, nesting-switch instead of an array of callables, and the Clang compiler will convert the nesting-switches into a jump table.

For example:

```c++
#include <utility>

namespace meta {

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
```

With almost the same example as above, but using `SwitchInstantiator` instead and providing a default implementation for `Message<X>::process`:

```c++
enum MessageType: std::size_t {
    INCREMENTAL = 3,
    TRADE = 5,
    INVALID = 7
};

template<std::size_t>
struct Message {
    static void process(const Message &);
};

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
    meta::SwitchInstantiator<
        MessageProcessor, INVALID, void(const void *)
    >::execute(data, id);
}
```

We see a much more complicated jump table, with much higher runtime costs, with some branching.  Again, the [compiler explorer](https://gcc.godbolt.org/#z:OYLghAFBqd5QCxAYwPYBMCmBRdBLAF1QCcAaPECAKxAEZSBnVAV2OUxAHIBSAJgGY8AO2QAbZlgDU3fgGFmBPKMIBPGdm4AGAIJbtQgIYBbTAwAOB9pJMED0gOwAhPXoKYjZ0QbczZeyQGSbh5ePnIMBOggIAx4AF6YAPoE6pJiBgwMkgCqDJjEACrunt6YpP6BEVEx8UkEkgDKteU6gUEqZpiGJo14wIYErJgu/Bo6EcTMyPUNAO6EyAgAkkIRBkKK3iQyzjquxaHDchUBwSVhslXRsQnJqemZOXmFB6Ut2m1XNbczzSftnW6mEkACVMINiEJ3m0CB0usZMAA6ZGSbTEYAMEZjbQTKYzeYERYrNYbPBbYi%2BXL5IohN69BKkUHg1hCCBojHIxEASlS3Cc/zWimQTIhQkkmAAHphkApMGz0QxOZIDArGV8bnVJMIsBKuQ5dh82oFiMzIZJ0Kh8r5NOoIF8AGYkWYq9C%2BdmY0YQFUYrmcxnayU8/gGtp8gAiLh0ZmIeAAbqUQP8zodfOraslJAA5dAS9QC2xCyRfeEAI1ESTw9rucmzEukcnpwIAtJJ6CKWXnWoELfl5RyUd6GGrItd0/UA7r9f9PgTFhAJ3q%2BSGjSvkBlgbXE12VzvJCbRU9qa8LrX1NFJdLZXaRyBHcRncRXXJ3bbB77kUHl7uAlh7QZmKIBBboa35Gj2FI1jm0i8I4ra2g6Toum6Cqvgq76Iv6Qg6p%2B06BOG/z4Xs25BMeRyXDeGoZqeoz5t4eDCsWhhlhWVYpJBdYyBG/Bho2kgtm2YKip2IHmpaxB9oqA6qkWFFjou/LEW0%2B4soeLy0ieOZniAF4ym417VHeD5PrIL6em%2BnI4cRhG6PYXEGi4tmRroOhdMwRiSAAsqYDAGMAmAFHCIAydUlH1Eu/xLJmsggtgHnYJmBTaAAMvWPH8NCgQFCC2hhtgqWSAArBlASRQAaslSw8Zxkj2A5dlOcmpSprJPzCbi0yed5vnAuFxGCvRkixqgeDoJI0aoOwmQQGgqz1F5mTddBABslk2fVRHaMwsRCMAkgrMgJomBsBiiPl80%2BX5viRdFsXxYlSV5sGehbcIu0FMQBhSNV53db4WU5RoNFPRtQ0jWNxATd5%2B2HV0tiiNNqCzXtIgw8dp18CtOx6KDo3jZNDDvZ9cozREkiE19vCY8DznaI1FxtQQkwdT9l1yNdMVxQlyW8gpIn9cKOPg5DU0k/U0PuLDJ3Leg3gGPJsF41DKMS2jEAy7Yn4OBGeyOdT%2BzqWRDNM3NXWs7I/25TzX4yXRAvDbjEP4wjSPkz1lPq3L%2BpC/jrtq7LmvWfhes6HTZFpj8nkFIysKAgi%2BU40beKdQtfkAAqO95JCJHgIRTn1BYDYLOlXqLg325IABUHvy4Huv2SHpG%2BDH8ImJyqQvTtZcjRm1UJ8HtON%2BELWah5BSJ8zpuYOnwtMMQ2chL4o%2BMjj1ayFgYjNxALNkaPWmK5kQaA3lvV8wXdtg8Xeml4LVf%2B3nIltNvi9j6M0T7wwEAVyawhuMQ0bgokNcERfClyfnIXeoxK62mrqtUMjkdbrRpqHZqIUxyR3HibFOU8M6ZBIEFbe098ZZxzp4Z%2BVsg7120ILd%2BztSY3w9phcc6Aa7ERsAYaIcwFjLFmusTYRAIJ%2BEUoEAhODZ6MjKhVMMy97a0PqDfHkVlX7aSlLpOUDCtTMKxggzgXJSCiC4AVTgpAhBcE0EY1AXA/AwT4LBJgrArB8H4LQIxQFOBmN0RAJALACBmAUOQSgaAPBKHyHQUgmB8D8LoLo/RnBDHGNMeYrgjjaCSAJAgSQEoAAcS0mxLQACxpC8J3AqiJNClJcQkrkuiADWIACr8ERLQAqtBaCaE0AATkyfYDptBMmZL0VwPJRiTFuMSZwIxDAQCaFIK49xpA4CwCQIEswwSyAUARkE8sxAUBFOAAVNppB7RKF/pMiAiRzmaEoLoksCTSCBKOgQAA8kIUQKhbn4BNNMOMphbk/0wDEsxpAqAAEdmD5DeZwBpvBGlGPLDtAgSB6D/1jI8ksVBpRATORcq5eiujAARaE5FqL0XTEoOcxIlzqAAEUwXEBUPwXgrSCrtM0Hk7pHS8mMsye0/ZLTKD4HMF4CFOjol4oJbQKpMyYxGEqaQVAZhFCI0mZwPijzeB8SqJxZANibG0AKU2AA6idUQEyWBsA4PQZ0GxHkKpWI6LgzjokGOGbcixnAsk5PyYU9Yu0SllM0JICAuBCAkGgk4xkshUCbPyGGiVFTRmStqXkpaiJMl5L6e0lltBeAssybQfgeSBmcCGfE0ZcquATKmTMypTrOC8BdWWt18b3G6NjPkWIiMQB5KAA%3D) shows it.

But **GCC does NOT generate a jump table!**, the nested switches are implemented as cascading if-then-elses:

```assembly
process(void const*, int):
  test esi, esi
  je .L21
  cmp esi, 1
  je .L22
  cmp esi, 2
  je .L23
  cmp esi, 3
  je .L24
  cmp esi, 4
  je .L25
  cmp esi, 5
  je .L26
  jmp Message<6ul>::process(Message<6ul> const&)
.L22:
  jmp Message<1ul>::process(Message<1ul> const&)
.L21:
  jmp Message<0ul>::process(Message<0ul> const&)
.L23:
  jmp Message<2ul>::process(Message<2ul> const&)
.L24:
  jmp processIncremental(Message<3ul> const&)
.L25:
  jmp Message<4ul>::process(Message<4ul> const&)
.L26:
  jmp processTrade(Message<5ul> const&)
  ```
  
