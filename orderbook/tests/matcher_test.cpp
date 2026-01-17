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

    Order makeStopLimitOrder(long traderId, long ordId, Side side, long qty, long price, long stopPrice, long timestamp){
        Order o{};
        o.traderId = traderId;
        o.ordId = ordId;
        o.side = side;
        o.qty = qty;
        o.price = price;
        o.stopPrice = stopPrice;
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

    Order makeStopOrder(long traderId, long ordId, Side side, long qty, long stopPrice, long timestamp){
        Order o{};
        o.traderId = traderId;
        o.ordId = ordId;
        o.side = side;
        o.qty = qty;
        o.price = 0;
        o.stopPrice = stopPrice;
        o.symbol = "TEST";
        o.type = STOP;
        o.timestamp = timestamp;
        return o;
    }


};

TEST_F(MatcherTest, EmptyBook_EmptySpread){
    auto spread = matcher.getSpread();
    EXPECT_TRUE(spread.asksMissing && spread.bidsMissing);
}


TEST_F(MatcherTest, AddSellLimit_PopulatesAsk){
    auto sell = makeLimitOrder(2, 2, SELL, 5, 900, 2);
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
    matcher.addOrder(ask);
    matcher.addOrder(bid);
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

TEST_F(MatcherTest, PlaceLimits_SpreadIsCorrect){
    std::vector<Order> orders = {
        makeLimitOrder(1, 1, BUY, 100, 5, 1),
        makeLimitOrder(2, 2, SELL, 100, 10, 2),
        makeLimitOrder(3, 3, BUY, 100, 6, 3),
        makeLimitOrder(4, 4, SELL, 100, 12, 4)
    };

    for(auto order : orders){
        matcher.addOrder(order);
    }

    EXPECT_EQ(orders.size(), notifier.placedOrders.size());
    EXPECT_EQ(0, notifier.matches.size());

    auto spread = matcher.getSpread();
    EXPECT_FALSE(spread.asksMissing || spread.bidsMissing);
    EXPECT_EQ(10, spread.lowestAsk);
    EXPECT_EQ(6, spread.highestBid);
}


TEST_F(MatcherTest, PlaceLimitsAndMarkets_MatchesAndSpreadAreCorrect){
    std::vector<Order> orders = {
        makeLimitOrder(1, 1, BUY, 100, 5, 1), // <- Sell-Market should half fill this guy
        makeLimitOrder(2, 2, SELL, 100, 10, 2), // <- Buy-Markets should fill this guy
        makeLimitOrder(3, 3, BUY, 100, 6, 3),  // <- Sell-Market should completely fill this guy
        makeLimitOrder(4, 4, SELL, 100, 12, 4),

        // Now place market orders
        makeMarketOrder(5, 5, BUY, 50, 5),
        makeMarketOrder(6, 6, BUY, 50, 6),
        makeMarketOrder(7, 7, SELL, 150, 7)
    };

    for(auto order : orders){
        matcher.addOrder(order);
    }

    // Check Notifier
    EXPECT_EQ(orders.size(), notifier.placedOrders.size());
    EXPECT_EQ(4, notifier.matches.size());
    EXPECT_EQ(orders[1].ordId, notifier.matches[0].seller.ordId); // Market-Buys
    EXPECT_EQ(orders[4].ordId, notifier.matches[0].buyer.ordId);
    EXPECT_EQ(50, notifier.matches[0].qty);
    EXPECT_EQ(orders[1].ordId, notifier.matches[1].seller.ordId);
    EXPECT_EQ(orders[5].ordId, notifier.matches[1].buyer.ordId);
    EXPECT_EQ(50, notifier.matches[1].qty);

    EXPECT_EQ(orders[6].ordId, notifier.matches[2].seller.ordId); // Market-Sell
    EXPECT_EQ(orders[2].ordId, notifier.matches[2].buyer.ordId);
    EXPECT_EQ(100, notifier.matches[2].qty);
    EXPECT_EQ(orders[6].ordId, notifier.matches[3].seller.ordId);
    EXPECT_EQ(orders[0].ordId, notifier.matches[3].buyer.ordId);
    EXPECT_EQ(50, notifier.matches[3].qty);

    // Check Spread
    auto spread = matcher.getSpread();
    EXPECT_FALSE(spread.asksMissing || spread.bidsMissing);
    EXPECT_EQ(12, spread.lowestAsk);
    EXPECT_EQ(5, spread.highestBid);
}

TEST_F(MatcherTest, PlaceStopLimitsAndStops_MatchesAndSpreadAreCorrect){
    std::vector<Order> orders = {
        
    };

    for(auto order : orders){
        matcher.addOrder(order);
    }

    // Check Notifier


    // Check Spread
    auto spread = matcher.getSpread();
    //EXPECT_FALSE(spread.asksMissing || spread.bidsMissing);
    //EXPECT_EQ(12, spread.lowestAsk);
    //EXPECT_EQ(5, spread.highestBid);
}