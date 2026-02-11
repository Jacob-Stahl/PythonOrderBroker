#include <gtest/gtest.h>
#include "../agent.h"

class ProducerTest : public ::testing::Test {
protected:
    long producerId = 1;
    std::string asset = "TEST_ASSET";
    unsigned short preferredPrice = 100;
};

TEST_F(ProducerTest, DecreasesWhenNoBids) {
    Producer producer(producerId, asset, preferredPrice);
    Observation obs;
    Spread spread;
    spread.bidsMissing = true;
    obs.assetSpreads[asset] = spread;
    obs.time = tick(10);
    
    // Empty observation (no spreads)
    Action act = producer.policy(obs);
    
    EXPECT_TRUE(act.placeOrder);
    EXPECT_EQ(act.order.qty, 0);
}

TEST_F(ProducerTest, IncreasesQtyWhenBidHigh) {
    Producer producer(producerId, asset, preferredPrice);
    Observation obs;
    obs.time = tick(10);
    
    Spread spread;
    spread.bidsMissing = false;
    spread.highestBid = preferredPrice + 10; // Higher than preferred
    obs.assetSpreads[asset] = spread;

    // First tick: qty starts at 1, should increase to 2?
    // Implementation: qtyPerTick default is 1. ++qtyPerTick happens before order creation.
    // So 1 -> 2.
    
    Action act = producer.policy(obs);
    
    EXPECT_TRUE(act.placeOrder);
    EXPECT_EQ(act.order.qty, 2);
    EXPECT_EQ(act.order.side, SELL);
    EXPECT_EQ(act.order.type, MARKET);
}

TEST_F(ProducerTest, DecreasesQtyWhenBidLow) {
    Producer producer(producerId, asset, preferredPrice);
    Observation obs;
    obs.time = tick(10);
    
    Spread spread;
    spread.bidsMissing = false;
    spread.highestBid = preferredPrice - 10; // Lower than preferred
    obs.assetSpreads[asset] = spread;

    // First tick: qty starts at 1. 1 -> 0?
    // Implementation: if (qtyPerTick > 0) --qtyPerTick;
    
    Action act = producer.policy(obs);
    
    EXPECT_TRUE(act.placeOrder);
    EXPECT_EQ(act.order.qty, 0); 
}

class ConsumerTest : public ::testing::Test {
protected:
    long consumerId = 2;
    std::string asset = "TEST_ASSET";
    unsigned short maxPrice = 200;
    tick appetiteCoef = tick(10);
};

TEST_F(ConsumerTest, FirstActionInitializesConsumptionTime) {
    Consumer consumer(consumerId, asset, maxPrice, appetiteCoef);
    Observation obs;
    obs.time = tick(100);

    // First call initializes lastConsumed to 100
    // Time since consumption = 0
    // Price = sigmoidHunger(0) = 0?
    // fast_sigmoid(0) = 0.
    
    Action act = consumer.policy(obs);
    
    EXPECT_TRUE(act.placeOrder);
    EXPECT_EQ(act.order.side, BUY);
    EXPECT_EQ(act.order.price, 0);
    EXPECT_FALSE(act.cancelOrder);
}

TEST_F(ConsumerTest, GetsHungrierOverTime) {
    Consumer consumer(consumerId, asset, maxPrice, appetiteCoef);
    Observation obs1;
    obs1.time = tick(100);
    consumer.policy(obs1); // Init lastConsumed = 100
    
    Observation obs2;
    obs2.time = tick(120); // 20 ticks later
    // appetiteCoef = 10.
    // x = 20 / 10 = 2.
    // fast_sigmoid(2) = 2 / (1+2) = 2/3.
    // price = 2/3 * 200 = 133.
    
    Action act = consumer.policy(obs2);
    
    EXPECT_TRUE(act.placeOrder);
    // Integer division in C++: 133
    EXPECT_NEAR(act.order.price, 133, 1);
}

TEST_F(ConsumerTest, CancelsPreviousOrder) {
    Consumer consumer(consumerId, asset, maxPrice, appetiteCoef);
    Observation obs1; 
    obs1.time = tick(100);
    
    consumer.policy(obs1);
    consumer.orderPlaced(555, tick(101)); // Simulate successful placement
    
    Observation obs2;
    obs2.time = tick(110);
    Action act = consumer.policy(obs2);
    
    EXPECT_TRUE(act.placeOrder);
    EXPECT_TRUE(act.cancelOrder);
    EXPECT_EQ(act.doomedOrderId, 555);
}

TEST_F(ConsumerTest, ConsumingResetsHunger) {
    Consumer consumer(consumerId, asset, maxPrice, appetiteCoef);
    Observation obs1;
    obs1.time = tick(100);
    consumer.policy(obs1); // init
    
    // Simulate Match
    Order buy(asset, BUY, LIMIT, 100, 1);
    Order sell(asset, SELL, LIMIT, 100, 1);
    Match match(buy, sell, 1);
    
    consumer.matchFound(match, tick(150));
    
    // Next policy call at 155
    Observation obs2;
    obs2.time = tick(155);
    // Time since = 5.
    // x = 5 / 10 = 0 (integer division of ticks).
    // wait, tick division likely calls raw() division.
    // 5/10 = 0.
    // sig(0) = 0.
    
    Action act = consumer.policy(obs2);
    // Time since = 5.
    // x = 5 / 10 = 0.5
    // sig(0.5) = 0.5 / 1.5 = 1/3
    // 1/3 * 200 = 66
    EXPECT_NEAR(act.order.price, 66, 1);
}
