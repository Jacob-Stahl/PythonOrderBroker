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
    std::vector<Match> matches;
    MockProducerAgent(long id) : Agent(id) {}
    Action policy(const Observation& obs) override {
        if(obs.time == tick(0)){
            Order o;
            o.traderId = traderId;
            o.ordId = traderId * 1000 + 1;
            o.side = Side::SELL;
            o.qty = 1;
            o.asset = "FOOD";
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
    std::vector<Match> matches;
    MockConsumerAgent(long id) : Agent(id) {}
    Action policy(const Observation& obs) override {
        if(obs.time == tick(0)){
            Order o;
            o.traderId = traderId;
            o.ordId = traderId * 1000 + 1;
            o.side = Side::BUY;
            o.qty = 1;
            o.price = 100;
            o.asset = "FOOD";
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
    long myOrderId = -1;
    bool cancellationConfirmed = false;
    long canceledOrderId = -1;

    CancelingAgent(long id) : Agent(id) {}

    Action policy(const Observation& obs) override {
        if(obs.time == tick(0)){
            Order o;
            o.traderId = traderId;
            o.ordId = traderId * 1000 + 1;
            myOrderId = o.ordId;
            o.side = Side::SELL;
            o.qty = 1;
            o.asset = "FOOD";
            o.type = OrdType::LIMIT;
            o.price = 100;
            return Action(o); 
        }
        if(obs.time == tick(1)){
             return Action(myOrderId);
        }
        return Action();
    }

    void orderCanceled(long orderId, tick now) override {
        cancellationConfirmed = true;
        canceledOrderId = orderId;
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
    EXPECT_EQ(pAgent->canceledOrderId, pAgent->myOrderId);

    // Verify order is gone from book
    obs = abm.getLatestObservation();
    depth = obs.assetOrderDepths.at("FOOD");
    EXPECT_TRUE(depth.askBins.empty());
}
