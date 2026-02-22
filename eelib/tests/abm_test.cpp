#include <gtest/gtest.h>
#include "../abm.h"
#include "../agent.h"

class MockAgent : public Agent {
public:
    MockAgent(long id) : Agent(id) {}
    
    Action policy(const Observation& observation) override {
        return Action();
    }
};

class MockSelector : public AgentSelector {
    long threshold;
public:
    MockSelector(long threshold_) : threshold(threshold_) {}
    
    bool keepThis(const std::unique_ptr<Agent>& agent) override {
        return agent->traderId < threshold;
    }
};

class ABMTest : public ::testing::Test {
protected:
    ABM abm;
};

TEST_F(ABMTest, AddAgentReturnsCorrectId) {
    auto agent = std::make_unique<MockAgent>(0); // Initial ID doesn't matter
    long id = abm.addAgent(std::move(agent));
    
    EXPECT_EQ(id, 1);
}

TEST_F(ABMTest, AddMultipleAgentsIncrementIds) {
    long id1 = abm.addAgent(std::make_unique<MockAgent>(0));
    long id2 = abm.addAgent(std::make_unique<MockAgent>(0));
    
    EXPECT_EQ(id1, 1);
    EXPECT_EQ(id2, 2);
}

TEST_F(ABMTest, RemoveAgentsBasedOnId) {
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 1
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 2
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 3
    abm.addAgent(std::make_unique<MockAgent>(0)); // ID 4
    
    EXPECT_EQ(abm.getNumAgents(), 4);
    
    // Keep agents with ID < 3. Should keep ID 1 and 2. Remove 3 and 4.
    MockSelector selector(3);
    abm.removeAgents(selector);
    
    EXPECT_EQ(abm.getNumAgents(), 2);
}

class MockProducerAgent : public Agent {
public:
    std::string asset;
    std::vector<Match> matches;
    MockProducerAgent(long id, std::string asset_ = "FOOD") : Agent(id), asset(asset_) {}
    Action policy(const Observation& obs) override {
        if(obs.time == tick(0)){
            Order o;
            o.traderId = traderId;
            o.ordId = traderId * 1000 + 1;
            o.side = Side::SELL;
            o.qty = 1;
            o.asset = asset;
            o.type = OrdType::MARKET;
            return Action(o); 
        }
        return Action();
    }
    void matchFound(const Match& match, tick now) override {
        matches.push_back(match);
    }
};

class MockConsumerAgent : public Agent {
public:
    std::string asset;
    std::vector<Match> matches;
    MockConsumerAgent(long id, std::string asset_ = "FOOD") : Agent(id), asset(asset_) {}
    Action policy(const Observation& obs) override {
        if(obs.time == tick(0)){
            Order o;
            o.traderId = traderId;
            o.ordId = traderId * 1000 + 1;
            o.side = Side::BUY;
            o.qty = 1;
            o.price = 100;
            o.asset = asset;
            o.type = OrdType::LIMIT;
            return Action(o);
        }
        return Action();
    }
    void matchFound(const Match& match, tick now) override {
        matches.push_back(match);
    }
};

TEST_F(ABMTest, ProducerConsumerOneStep) {
    // 1 Producer, 3 Consumers
    abm.addAgent(std::make_unique<MockProducerAgent>(0));
    abm.addAgent(std::make_unique<MockConsumerAgent>(0));
    abm.addAgent(std::make_unique<MockConsumerAgent>(0));
    abm.addAgent(std::make_unique<MockConsumerAgent>(0));

    // Run one iteration of simStep()
    // This executes the orders.
    abm.simStep();

    auto obs = abm.getLatestObservation();
    
    // Check tick counter via observation time
    // First step: time 0. Second step: time 1.
    EXPECT_EQ(obs.time, tick(1));

    // Make sure the FOOD order book is in an expected state
    ASSERT_TRUE(obs.assetOrderDepths.count("FOOD"));
    Depth depth = obs.assetOrderDepths.at("FOOD");

    // Producer (Market Sell 1) should match with one Consumer (Limit Buy 1).
    // 3 Consumers total -> 3 Bids.
    // 1 Bid matched -> 2 Bids left.
    // No Asks left (Producer filled).

    ASSERT_EQ(depth.bidBins.size(), 1);
    EXPECT_EQ(depth.bidBins[0].price, 100);
    EXPECT_EQ(depth.bidBins[0].totalQty, 2); // 2 remaining

    EXPECT_TRUE(depth.askBins.empty());
}

TEST_F(ABMTest, MultipleStepsIncrementTickCounter) {
    int numSteps = 10;
    for(int i = 0; i < numSteps; ++i) {
        abm.simStep();
        // simStep increments tickCounter and calls observe() at the end
        EXPECT_EQ(abm.getLatestObservation().time, tick(i + 1)); 
    }
}

TEST_F(ABMTest, MatchRoutingToAgents) {
    // 1 Producer, 1 Consumer
    auto producer = std::make_unique<MockProducerAgent>(0);
    auto consumer = std::make_unique<MockConsumerAgent>(0);

    // Keep raw pointers to check state later
    MockProducerAgent* pProducer = producer.get();
    MockConsumerAgent* pConsumer = consumer.get();

    abm.addAgent(std::move(producer));
    abm.addAgent(std::move(consumer));

    // Run one iteration of simStep()
    abm.simStep();

    // Verify matches
    ASSERT_EQ(pProducer->matches.size(), 1);
    ASSERT_EQ(pConsumer->matches.size(), 1);

    Match prodMatch = pProducer->matches[0];
    Match consMatch = pConsumer->matches[0];

    // Check quantities
    EXPECT_EQ(prodMatch.qty, 1);
    EXPECT_EQ(consMatch.qty, 1);

    // Check trader Ids identify who was who
    EXPECT_EQ(prodMatch.seller.traderId, pProducer->traderId);
    EXPECT_EQ(prodMatch.buyer.traderId, pConsumer->traderId);
    
    EXPECT_EQ(consMatch.seller.traderId, pProducer->traderId);
    EXPECT_EQ(consMatch.buyer.traderId, pConsumer->traderId);
}

class CancelingAgent : public Agent {
public:
    bool cancellationConfirmed = false;
    long orderToCancel = -1;

    CancelingAgent(long id) : Agent(id) {}

    Action policy(const Observation& obs) override {

        // Place an order at tick 0, cancel it at tick 1
        if(obs.time == tick(0)){
            Order o;
            o.traderId = traderId;
            o.ordId = traderId * 1000 + 1;
            o.side = Side::SELL;
            o.qty = 1;
            o.asset = "FOOD";
            o.type = OrdType::LIMIT;
            o.price = 100;
            return Action(o); 
        }
        if(obs.time == tick(1)){
             return Action(orderToCancel); // Cancel the order placed at tick 0
        }
        return Action();
    }

    void orderPlaced(long orderId, tick now) override {
        orderToCancel = orderId; // Store the order ID to cancel later
    }

    void orderCanceled(long orderId, tick now) override {
        cancellationConfirmed = true;
        orderToCancel = orderId;
    }
};

TEST_F(ABMTest, CancellationRouting) {
    auto agent = std::make_unique<CancelingAgent>(0);
    CancelingAgent* pAgent = agent.get();
    abm.addAgent(std::move(agent));

    // Tick 0 -> 1: Place Order
    abm.simStep();

    // Verify order is on the book
    auto obs = abm.getLatestObservation();
    Depth depth = obs.assetOrderDepths.at("FOOD");
    ASSERT_EQ(depth.askBins.size(), 1);
    EXPECT_EQ(depth.askBins[0].totalQty, 1);

    // Tick 1 -> 2: Cancel Order
    abm.simStep();

    // Verify cancellation callback
    EXPECT_TRUE(pAgent->cancellationConfirmed);

    // Verify order is gone from book
    obs = abm.getLatestObservation();
    depth = obs.assetOrderDepths.at("FOOD");
    EXPECT_TRUE(depth.askBins.empty());
}

TEST_F(ABMTest, MultipleAssetsNoCrossTalk) {
    // Create 1 producer and 1 consumer for FOOD
    auto foodProducer = std::make_unique<MockProducerAgent>(0, "FOOD");
    auto foodConsumer = std::make_unique<MockConsumerAgent>(0, "FOOD");
    MockProducerAgent* pFoodProd = foodProducer.get();
    MockConsumerAgent* pFoodCons = foodConsumer.get();

    // Create 1 producer and 1 consumer for WATER
    auto waterProducer = std::make_unique<MockProducerAgent>(0, "WATER");
    auto waterConsumer = std::make_unique<MockConsumerAgent>(0, "WATER");
    MockProducerAgent* pWaterProd = waterProducer.get();
    MockConsumerAgent* pWaterCons = waterConsumer.get();

    abm.addAgent(std::move(foodProducer));
    abm.addAgent(std::move(foodConsumer));
    abm.addAgent(std::move(waterProducer));
    abm.addAgent(std::move(waterConsumer));

    abm.simStep();

    // Verify matches for FOOD agents
    ASSERT_EQ(pFoodProd->matches.size(), 1);
    ASSERT_EQ(pFoodCons->matches.size(), 1);

    // Verify matches for WATER agents
    ASSERT_EQ(pWaterProd->matches.size(), 1);
    ASSERT_EQ(pWaterCons->matches.size(), 1);

    // Check assets in match details
    EXPECT_EQ(pFoodProd->matches[0].buyer.asset, "FOOD");
    EXPECT_EQ(pFoodProd->matches[0].seller.asset, "FOOD");
    
    EXPECT_EQ(pWaterProd->matches[0].buyer.asset, "WATER");
    EXPECT_EQ(pWaterProd->matches[0].seller.asset, "WATER");

    // Verify correct partners (no cross-talk)
    EXPECT_EQ(pFoodProd->matches[0].buyer.traderId, pFoodCons->traderId);
    EXPECT_EQ(pFoodCons->matches[0].seller.traderId, pFoodProd->traderId);

    EXPECT_EQ(pWaterProd->matches[0].buyer.traderId, pWaterCons->traderId);
    EXPECT_EQ(pWaterCons->matches[0].seller.traderId, pWaterProd->traderId);
}

class TickSpyAgent : public Agent {
public:
    tick lastOrderPlacedTick;
    tick lastOrderCanceledTick;
    tick lastMatchFoundTick;
    bool orderPlacedCalled = false;
    bool orderCanceledCalled = false;
    bool matchFoundCalled = false;

    Action nextAction;

    TickSpyAgent(long id) : Agent(id) {}

    Action policy(const Observation& obs) override {
        return nextAction;
    }

    void orderPlaced(long orderId, tick now) override {
        lastOrderPlacedTick = now;
        orderPlacedCalled = true;
    }

    void orderCanceled(long orderId, tick now) override {
        lastOrderCanceledTick = now;
        orderCanceledCalled = true;
    }

    void matchFound(const Match& match, tick now) override {
        lastMatchFoundTick = now;
        matchFoundCalled = true;
    }
};

TEST_F(ABMTest, AgentReceivesCorrectTickOnEvents) {
    auto agentPtr = std::make_unique<TickSpyAgent>(0);
    TickSpyAgent* agent = agentPtr.get();
    abm.addAgent(std::move(agentPtr));

    // Tick 0: Place Order
    Order order("ASSET", BUY, LIMIT, 100, 1);
    order.ordId = 123;
    agent->nextAction = Action(order);
    
    abm.simStep(); // Tick becomes 1
    
    EXPECT_TRUE(agent->orderPlacedCalled);
    EXPECT_EQ(agent->lastOrderPlacedTick.raw(), 0);

    // Tick 1: Cancel Order
    agent->nextAction = Action(order.ordId); // Cancel previous order
    agent->orderPlacedCalled = false; // Reset flags
    
    abm.simStep(); // Tick becomes 2
    
    EXPECT_TRUE(agent->orderCanceledCalled);
    EXPECT_EQ(agent->lastOrderCanceledTick.raw(), 1);

    // Tick 2: Place Another Order
    agent->orderPlacedCalled = false;
    order.ordId = 124;
    agent->nextAction = Action(order);

    abm.simStep(); // Tick becomes 3

    EXPECT_TRUE(agent->orderPlacedCalled);
    EXPECT_EQ(agent->lastOrderPlacedTick.raw(), 2);
}

TEST_F(ABMTest, AgentReceivesCorrectTickOnMatch) {
    // Producer/Consumer scenario to trigger a match
    auto producerPtr = std::make_unique<TickSpyAgent>(0);
    TickSpyAgent* producer = producerPtr.get();
    
    auto consumerPtr = std::make_unique<TickSpyAgent>(0);
    TickSpyAgent* consumer = consumerPtr.get();

    abm.addAgent(std::move(producerPtr));
    abm.addAgent(std::move(consumerPtr));

    // Tick 0: Producer places SELL
    Order sellOrder("ASSET", SELL, LIMIT, 100, 1);
    sellOrder.ordId = 1;
    sellOrder.traderId = producer->traderId;
    producer->nextAction = Action(sellOrder);
    consumer->nextAction = Action(); // Do nothing
    
    abm.simStep(); // Tick 0 -> 1

    EXPECT_TRUE(producer->orderPlacedCalled);
    EXPECT_EQ(producer->lastOrderPlacedTick.raw(), 0);

    // Tick 1: Consumer places BUY -> MATCH
    producer->nextAction = Action();
    
    Order buyOrder("ASSET", BUY, MARKET, 0, 1);
    buyOrder.ordId = 2;
    buyOrder.traderId = consumer->traderId;
    consumer->nextAction = Action(buyOrder);

    abm.simStep(); // Tick 1 -> 2

    // Producer receives match
    EXPECT_TRUE(producer->matchFoundCalled);
    EXPECT_EQ(producer->lastMatchFoundTick.raw(), 1);

    // Consumer receives match
    EXPECT_TRUE(consumer->matchFoundCalled);
    EXPECT_EQ(consumer->lastMatchFoundTick.raw(), 1);
    
    // Check placed tick for consumer
    EXPECT_TRUE(consumer->orderPlacedCalled);
    EXPECT_EQ(consumer->lastOrderPlacedTick.raw(), 1);

    // Tick 2: Step Forward, verify tick increments
    producer->orderPlacedCalled = false;
    sellOrder.ordId = 3;
    producer->nextAction = Action(sellOrder);
    consumer->nextAction = Action();

    abm.simStep(); // Tick 2 -> 3

    EXPECT_TRUE(producer->orderPlacedCalled);
    EXPECT_EQ(producer->lastOrderPlacedTick.raw(), 2);
}
