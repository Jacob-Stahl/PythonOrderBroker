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

class ABMTest : public ::testing::Test {
protected:
    ABM abm;
};

TEST_F(ABMTest, AddAgentReturnsCorrectId) {
    auto agent = std::make_unique<MockAgent>(0); // Initial ID doesn't matter
    long id = abm.addAgent(std::move(agent));
    
    // First agent should have ID 2 (nextTraderId starts at 1 and is incremented before assignment)
    EXPECT_EQ(id, 2);
}

TEST_F(ABMTest, AddMultipleAgentsIncrementIds) {
    long id1 = abm.addAgent(std::make_unique<MockAgent>(0));
    long id2 = abm.addAgent(std::make_unique<MockAgent>(0));
    
    EXPECT_EQ(id1, 2);
    EXPECT_EQ(id2, 3);
}
