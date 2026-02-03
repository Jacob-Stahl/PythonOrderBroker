#include <gtest/gtest.h>

#include "matcher.h"
#include "notifier.h"
#include "order.h"

struct MatcherTest : ::testing::Test {
    MockNotifier notifier;
    Matcher matcher{&notifier};

    long int lastOrdNum = 1;

    Order& newOrder(Side side, OrdType type, long qty, long price = 0, long stopPrice = 0){
        Order* o = new Order{};
        o->traderId = lastOrdNum;
        o->ordId = lastOrdNum;
        o->side = side;
        o->qty = qty;
        o->price = price;
        o->stopPrice = stopPrice;
        o->symbol = "TEST";
        o->type = type;

        ++lastOrdNum;

        // This will cause a leak. Probably fine for a unit test helper
        return *o;
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


TEST_F(MatcherTest, MatchLimitsAndMarkets_MatchesAndSpreadAreCorrect){
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

    // Check Matches
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

TEST_F(MatcherTest, MatchStopLimits_MatchesAndSpreadAreCorrect){
    std::vector<Order> orders = {
        newOrder(BUY, STOPLIMIT,  420, 60, 70), // <- Irrational stop order will get rejected

        newOrder(SELL, LIMIT,     100, 60),
        newOrder(BUY, STOPLIMIT,  100, 50, 45),      // <- 3rd, this is only matched when the highest ask moves above the stop price
        newOrder(SELL, LIMIT,     100, 40),          // <- 2nd, price moves above STOP price for STOPLIMIT after this is matched

        newOrder(BUY, LIMIT, 100, 20),               // <- 1st, even though the STOPLIMIT above has a higher offer, we are below the STOP price
        
        newOrder(SELL, MARKET, 100), // Match BUY  LIMIT
        newOrder(BUY,  MARKET, 100), // Match SELL LIMIT
        newOrder(SELL, MARKET, 100), // Match BUY  STOPLIMIT
    };

    for(auto order : orders){
        matcher.addOrder(order);
    }

    // Check Notifier
    EXPECT_EQ(1, notifier.placementFailedOrders.size());
    EXPECT_EQ(7, notifier.placedOrders.size());
    EXPECT_EQ(3, notifier.matches.size());

    // Check Matches
    EXPECT_EQ(orders[4].ordId, notifier.matches[0].buyer.ordId); // SELL MARKET
    EXPECT_EQ(orders[5].ordId, notifier.matches[0].seller.ordId);

    EXPECT_EQ(orders[6].ordId, notifier.matches[1].buyer.ordId); // BUY MARKET
    EXPECT_EQ(orders[3].ordId, notifier.matches[1].seller.ordId);

    EXPECT_EQ(orders[7].ordId, notifier.matches[2].seller.ordId); // SELL MARKET matches with the  BUY STOP LIMIT
    EXPECT_EQ(orders[2].ordId, notifier.matches[2].buyer.ordId);

    // Check Spread
    auto spread = matcher.getSpread();
    EXPECT_EQ(60, spread.lowestAsk);
    EXPECT_TRUE(spread.bidsMissing);

}

TEST_F(MatcherTest, SellStop_TriggersAfterWittlingBuys){
    // Place several buy limit orders
    Order buy1 = newOrder(BUY, LIMIT, 50, 100);
    Order buy2 = newOrder(BUY, LIMIT, 50, 90);
    Order buy3 = newOrder(BUY, LIMIT, 50, 80);

    // Place a sell STOP with stop price in the middle (90)
    Order sellStop = newOrder(SELL, STOP, 50, 0, 90);

    matcher.addOrder(buy1);
    matcher.addOrder(buy2);
    matcher.addOrder(buy3);
    matcher.addOrder(sellStop);

    // No matches yet; stop should be inactive
    EXPECT_EQ(4, notifier.placedOrders.size());
    EXPECT_EQ(0, notifier.matches.size());

    // Use SELL MARKET orders to eat into the buy ladder
    matcher.addOrder(newOrder(SELL, MARKET, 50)); // consumes buy1 @100
    EXPECT_GE(notifier.matches.size(), 1);

    matcher.addOrder(newOrder(SELL, MARKET, 50)); // consumes buy2 @90 -> should trigger stop

    // After wittling down buys, the SELL STOP should have executed and produced at least one match
    bool stopExecuted = false;
    for(auto &m : notifier.matches){
        if(m.seller.ordId == sellStop.ordId){ stopExecuted = true; break; }
    }

    EXPECT_TRUE(stopExecuted);
}

TEST_F(MatcherTest, DumpOrdersTo_ExcludesCompletelyFilledOrders){
    // Place a buy limit that will be completely filled
    auto buyLimit1 = newOrder(BUY, LIMIT, 100, 10);
    // Place a sell market that will match and remove the buy limit
    auto sellMarket = newOrder(SELL, MARKET, 100);

    // Place additional limits that should remain (one will be partially filled)
    auto buyLimit2 = newOrder(BUY, LIMIT, 50, 5);
    auto sellLimit = newOrder(SELL, LIMIT, 30, 15);

    matcher.addOrder(buyLimit1);
    matcher.addOrder(sellMarket); // consumes buyLimit1 entirely
    matcher.addOrder(buyLimit2);
    matcher.addOrder(sellLimit);

    // Partially consume the sell limit (leaves it with remaining qty)
    matcher.addOrder(newOrder(BUY, MARKET, 10));

    // Dump remaining orders
    std::vector<Order> dumped;
    matcher.dumpOrdersTo(dumped);

    bool foundBuyLimit1 = false;
    bool foundSellMarket = false;
    bool foundBuyLimit2 = false;
    bool foundSellLimit = false;
    for (auto &o : dumped){
        if (o.ordId == buyLimit1.ordId) foundBuyLimit1 = true;
        if (o.ordId == sellMarket.ordId) foundSellMarket = true;
        if (o.ordId == buyLimit2.ordId) foundBuyLimit2 = true;
        if (o.ordId == sellLimit.ordId){
            foundSellLimit = true;
            EXPECT_GT(o.fill, 0);    // it was partially filled
            EXPECT_LT(o.fill, o.qty); // still has remaining qty
        }
    }

    // Completely filled orders should NOT be present
    EXPECT_FALSE(foundBuyLimit1);
    EXPECT_FALSE(foundSellMarket);

    // Partially filled / unfilled limits should be present
    EXPECT_TRUE(foundBuyLimit2);
    EXPECT_TRUE(foundSellLimit);
}

TEST_F(MatcherTest, GetOrderCounts_ReturnsCorrectCounts){
    auto o1 = newOrder(BUY, MARKET, 1); // Should be matched with STOPLIMIT
    auto o2 = newOrder(BUY, LIMIT, 1, 100);
    auto o3 = newOrder(SELL, STOP, 1, 0, 50);
    auto o4 = newOrder(SELL, STOPLIMIT, 1, 200, 210); // Should be matched with MARKET
    auto o5 = newOrder(BUY, LIMIT, 1, 120);

    matcher.addOrder(o1);
    matcher.addOrder(o2);
    matcher.addOrder(o3);
    matcher.addOrder(o4);
    matcher.addOrder(o5);

    EXPECT_EQ(1, notifier.matches.size());
    EXPECT_EQ(5, notifier.placedOrders.size());
    EXPECT_EQ(0, notifier.placementFailedOrders.size());

    auto counts = matcher.getOrderCounts();
    EXPECT_EQ(0, counts.at(MARKET));
    EXPECT_EQ(2, counts.at(LIMIT));
    EXPECT_EQ(1, counts.at(STOP));
    EXPECT_EQ(0, counts.at(STOPLIMIT));
}

TEST_F(MatcherTest, CancelAllOrderTypes){
    auto market    = newOrder(BUY, MARKET, 10);
    auto limit     = newOrder(SELL, LIMIT, 10, 100);
    auto stop      = newOrder(BUY, STOP, 10, 0, 50);
    auto stoplimit = newOrder(SELL, STOPLIMIT, 10, 110, 120);

    // Add without immediate matching
    matcher.addOrder(market, false);
    matcher.addOrder(limit, false);
    matcher.addOrder(stop, false);
    matcher.addOrder(stoplimit, false);

    // Cancel all four
    matcher.cancelOrder(market.ordId);
    matcher.cancelOrder(limit.ordId);
    matcher.cancelOrder(stop.ordId);
    matcher.cancelOrder(stoplimit.ordId);

    // Trigger matching/cleanup by placing small market orders
    matcher.addOrder(newOrder(BUY, MARKET, 1));
    matcher.addOrder(newOrder(SELL, MARKET, 1));

    // Dump remaining orders and ensure canceled ones are not present
    std::vector<Order> dumped;
    matcher.dumpOrdersTo(dumped);

    for (auto &o : dumped){
        EXPECT_NE(o.ordId, market.ordId);
        EXPECT_NE(o.ordId, limit.ordId);
        EXPECT_NE(o.ordId, stop.ordId);
        EXPECT_NE(o.ordId, stoplimit.ordId);
    }

    // Ensure no match involves any of the canceled orders
    for (auto &m : notifier.matches){
        EXPECT_NE(m.buyer.ordId, market.ordId);
        EXPECT_NE(m.seller.ordId, limit.ordId);
        EXPECT_NE(m.buyer.ordId, stop.ordId);
        EXPECT_NE(m.seller.ordId, stoplimit.ordId);
    }
}