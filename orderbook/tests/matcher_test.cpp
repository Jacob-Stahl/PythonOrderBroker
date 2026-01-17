#include <gtest/gtest.h>

#include "matcher.h"
#include "notifier.h"
#include "order.h"

struct MatcherTest : ::testing::Test {
    MockNotifier notifier;
    Matcher matcher{&notifier};

    long int currentIdTimestamp = 1;

    Order newOrder(Side side, OrdType type, long qty, long price = 0, long stopPrice = 0){
        Order o{};
        o.traderId = currentIdTimestamp;
        o.ordId = currentIdTimestamp;
        o.side = side;
        o.qty = qty;
        o.price = price;
        o.stopPrice = stopPrice;
        o.symbol = "TEST";
        o.type = type;
        o.timestamp = currentIdTimestamp;

        ++currentIdTimestamp;
        return o;
    }

};

TEST_F(MatcherTest, EmptyBook_EmptySpread){
    auto spread = matcher.getSpread();
    EXPECT_TRUE(spread.asksMissing && spread.bidsMissing);
}


TEST_F(MatcherTest, AddSellLimit_PopulatesAsk){
    auto sell = newOrder(SELL, LIMIT, 5, 900);
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
    auto buy = newOrder(BUY, LIMIT, 5, 900);
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
    auto order = newOrder(BUY, MARKET, 5);
    matcher.addOrder(order);
    auto spread = matcher.getSpread();
    EXPECT_TRUE(spread.asksMissing && spread.bidsMissing);

    EXPECT_EQ(1, notifier.placedOrders.size());
    EXPECT_EQ(order.ordId, notifier.placedOrders[0].ordId);
    EXPECT_EQ(MARKET, notifier.placedOrders[0].type);
    EXPECT_EQ(BUY, notifier.placedOrders[0].side);
}

TEST_F(MatcherTest, AddSellMarket_PopulatesMarketOrders){
    auto order = newOrder(SELL, MARKET, 5);
    matcher.addOrder(order);
    auto spread = matcher.getSpread();
    EXPECT_TRUE(spread.asksMissing && spread.bidsMissing);

    EXPECT_EQ(1, notifier.placedOrders.size());
    EXPECT_EQ(order.ordId, notifier.placedOrders[0].ordId);
    EXPECT_EQ(MARKET, notifier.placedOrders[0].type);
    EXPECT_EQ(SELL, notifier.placedOrders[0].side);
}

TEST_F(MatcherTest, BuyLimit_Match_SellMarket){
    auto ask = newOrder(SELL, MARKET, 5);
    auto bid = newOrder(BUY, LIMIT, 5, 250);
    matcher.addOrder(ask);
    matcher.addOrder(bid);
    auto spread = matcher.getSpread();

    EXPECT_EQ(2, notifier.placedOrders.size());
    EXPECT_EQ(1, notifier.matches.size());

    EXPECT_TRUE(spread.asksMissing && spread.bidsMissing);
}

TEST_F(MatcherTest, SellLimit_Match_BuyMarket){
    auto bid = newOrder(BUY, MARKET, 5);
    auto ask = newOrder(SELL, LIMIT, 5, 250);
    matcher.addOrder(bid);
    matcher.addOrder(ask);
    auto spread = matcher.getSpread();

    EXPECT_EQ(2, notifier.placedOrders.size());
    EXPECT_EQ(1, notifier.matches.size());

    EXPECT_TRUE(spread.asksMissing && spread.bidsMissing);
}

TEST_F(MatcherTest, PlaceLimits_SpreadIsCorrect){
    std::vector<Order> orders = {
        newOrder(BUY, LIMIT, 100, 5),
        newOrder(SELL, LIMIT, 100, 10),
        newOrder(BUY, LIMIT, 100, 6),
        newOrder(SELL, LIMIT, 100, 12)
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
        newOrder(BUY, LIMIT, 100, 5), // <- Sell-Market should half fill this guy
        newOrder(SELL, LIMIT, 100, 10), // <- Buy-Markets should fill this guy
        newOrder(BUY, LIMIT, 100, 6),  // <- Sell-Market should completely fill this guy
        newOrder(SELL, LIMIT, 100, 12),

        // Now place market orders
        newOrder(BUY, MARKET, 50),
        newOrder(BUY, MARKET, 50),
        newOrder(SELL, MARKET, 150)
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
        newOrder(BUY, STOPLIMIT, 100, 50, 45),
        newOrder(SELL, LIMIT, 100, 40),
        newOrder(BUY, LIMIT, 100, 35),
        newOrder(SELL, STOPLIMIT, 100, 30, 32),
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