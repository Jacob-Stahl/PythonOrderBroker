#include <gtest/gtest.h>

#include "matcher.h"
#include "notifier.h"
#include "order.h"

struct MatcherTest : ::testing::Test {
    MockNotifier notifier;
    Matcher matcher{&notifier};

    Order makeLimitOrder(long traderId, long ordId, Side side, long qty, long price, long timestamp){
        Order o{};
        o.traderId = traderId;
        o.ordId = ordId;
        o.side = side;
        o.qty = qty;
        o.price = price;
        o.stopPrice = 0;
        o.symbol = "TEST";
        o.type = LIMIT;
        o.timestamp = timestamp;
        return o;
    }

    Order makeMarketOrder(long traderId, long ordId, Side side, long qty, long timestamp){
        Order o{};
        o.traderId = traderId;
        o.ordId = ordId;
        o.side = side;
        o.qty = qty;
        o.price = 0;
        o.stopPrice = 0;
        o.symbol = "TEST";
        o.type = MARKET;
        o.timestamp = timestamp;
        return o;
    }
};

TEST_F(MatcherTest, AddSellLimit_PopulatesAsk)
{
    auto sell = makeLimitOrder(1, 1, SELL, 10, 1000, 1);
    matcher.addOrder(sell);
    auto spread = matcher.getSpread();
    EXPECT_FALSE(spread.asksMissing);
}

TEST_F(MatcherTest, AddBuyLimit_PopulatesBid)
{
    auto buy = makeLimitOrder(2, 2, BUY, 5, 900, 2);
    matcher.addOrder(buy);
    auto spread = matcher.getSpread();
    EXPECT_FALSE(spread.bidsMissing);
}
