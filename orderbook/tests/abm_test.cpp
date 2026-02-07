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
