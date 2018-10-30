#include <meta/PackIndexer.h>
#include <meta/Instantiator.h>

using namespace meta;

enum QuoteSide {
    BID, ASK
};

enum LiquidityProvider {
    OUTRIGHT,
    IMPLIED
};

template<
    bool TopOfTheBook,
    QuoteSide S,
    LiquidityProvider LP
>
struct Quote;

struct Trade;

struct Uptick;


void unimplemented();
template<bool TopOrNot, QuoteSide QS, LiquidityProvider LP>
void processMarketMessage(Quote<TopOrNot, QS, LP> &) { unimplemented(); }

unsigned tradesSeen = 0;
void processMarketMessage(Trade &) { ++tradesSeen; }

template<typename T>
void processMarketMessage(T &) {}

template<typename Message>
struct ExchangeMessageProcessor {
    static void execute(void *arg) {
        processMarketMessage(*static_cast<Message *>(arg));
    }
};

using MessageTypeArray =
    Pack<
        Quote<false, BID, OUTRIGHT>,
        Quote<false, BID, IMPLIED>,
        Quote<false, ASK, OUTRIGHT>,
        Quote<false, ASK, IMPLIED>,
        Quote<true, BID, OUTRIGHT>,
        Quote<true, BID, IMPLIED>,
        Quote<true, ASK, OUTRIGHT>,
        Quote<true, ASK, IMPLIED>,
        Trade,
        void *,
        void *,
        void *,
        Uptick
    >;

void jumpTable(void *data, int index) {
    Instantiator<
        PackIndexer<ExchangeMessageProcessor, MessageTypeArray>::Internal,
        13,
        void(void *)
    >::execute(data, index);
}

