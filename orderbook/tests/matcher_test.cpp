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

TEST_F(MatcherTest, AddSellLimit_PopulatesAsk){
    auto sell = makeLimitOrder(1, 1, SELL, 10, 1000, 1);
    matcher.addOrder(sell);
    auto spread = matcher.getSpread();
    EXPECT_FALSE(spread.asksMissing);
    EXPECT_EQ(sell.price, spread.lowestAsk);
    EXPECT_TRUE(spread.bidsMissing);
    
    EXPECT_EQ(1, notifier.placedOrders.size());
    EXPECT_EQ(sell.ordId, notifier.placedOrders[0].ordId);
    EXPECT_EQ(LIMIT, notifier.placedOrders[0].type);
}

TEST_F(MatcherTest, AddBuyLimit_PopulatesBid){
    auto buy = makeLimitOrder(2, 2, BUY, 5, 900, 2);
    matcher.addOrder(buy);
    auto spread = matcher.getSpread();
    EXPECT_FALSE(spread.bidsMissing);
    EXPECT_EQ(buy.price, spread.highestBid);
    EXPECT_TRUE(spread.asksMissing);

    EXPECT_EQ(1, notifier.placedOrders.size());
    EXPECT_EQ(buy.ordId, notifier.placedOrders[0].ordId);
    EXPECT_EQ(LIMIT, notifier.placedOrders[0].type);
}

TEST_F(MatcherTest, AddBuyMarket_PopulatesMarketOrders){
    auto order = makeMarketOrder(1, 1, BUY, 5, 1);
    matcher.addOrder(order);
    auto spread = matcher.getSpread();
    EXPECT_TRUE(spread.asksMissing && spread.bidsMissing);

    EXPECT_EQ(1, notifier.placedOrders.size());
    EXPECT_EQ(order.ordId, notifier.placedOrders[0].ordId);
    EXPECT_EQ(MARKET, notifier.placedOrders[0].type);
    EXPECT_EQ(BUY, notifier.placedOrders[0].side);
}

TEST_F(MatcherTest, AddSellMarket_PopulatesMarketOrders){
    auto order = makeMarketOrder(1, 1, SELL, 5, 1);
    matcher.addOrder(order);
    auto spread = matcher.getSpread();
    EXPECT_TRUE(spread.asksMissing && spread.bidsMissing);

    EXPECT_EQ(1, notifier.placedOrders.size());
    EXPECT_EQ(order.ordId, notifier.placedOrders[0].ordId);
    EXPECT_EQ(MARKET, notifier.placedOrders[0].type);
    EXPECT_EQ(SELL, notifier.placedOrders[0].side);
}

TEST_F(MatcherTest, BuyLimit_Match_SellMarket){
    auto ask = makeMarketOrder(1, 1, SELL, 5, 1);
    auto bid = makeLimitOrder(2, 2, BUY, 5, 250, 2);
    matcher.addOrder(bid);
    matcher.addOrder(ask);
    auto spread = matcher.getSpread();

    EXPECT_EQ(2, notifier.placedOrders.size());
    EXPECT_EQ(1, notifier.matches.size());

    EXPECT_TRUE(spread.asksMissing && spread.bidsMissing);
}

TEST_F(MatcherTest, SellLimit_Match_BuyMarket){
    auto bid = makeMarketOrder(1, 1, BUY, 5, 1);
    auto ask = makeLimitOrder(2, 2, SELL, 5, 250, 2);
    matcher.addOrder(bid);
    matcher.addOrder(ask);
    auto spread = matcher.getSpread();

    EXPECT_EQ(2, notifier.placedOrders.size());
    EXPECT_EQ(1, notifier.matches.size());

    EXPECT_TRUE(spread.asksMissing && spread.bidsMissing);
}