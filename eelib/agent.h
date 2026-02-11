#pragma once

#include "matcher.h"
#include "match.h"
#include <string>
#include <functional>
#include <map>
#include "tick.h"

struct Observation{
    tick time;

    /// @brief asset - Spread
    std::map<std::string, Spread> assetSpreads;
    std::map<std::string, Depth> assetOrderDepths;
};

struct Action{
    bool placeOrder = false;
    Order order;

    bool cancelOrder = false;
    long doomedOrderId;

    Action() = default;

    Action(Order& order_){
        order = order_,
        placeOrder = true;
    }

    Action(Order& order_, long doomedOrderId_){
        placeOrder = true;
        order = order_;
        cancelOrder = true;
        doomedOrderId = doomedOrderId_;
    }

    Action(long doomedOrderId_){
        cancelOrder = true;
        doomedOrderId = doomedOrderId_;
    }
};

class Agent{
    public:
        long traderId;
        Agent(long);
        virtual ~Agent() = default;

        virtual Action policy(const Observation& observation);

        virtual void matchFound(const Match& match, tick now){};
        virtual void orderPlaced(long orderId, tick now){};
        virtual void orderCanceled(long orderId, tick now){};

        /// @brief Final action before agent is removed from ABM
        virtual Action lastWill(const Observation& observation){return Action();};
};

class Consumer : public Agent{

    private:
        std::string asset;
        tick lastConsumed;
        long lastPlacedOrderId;
        unsigned short maxPrice;
        tick ticksUntilHalfHunger;

        unsigned short newLimitPrice(tick now);
        unsigned short sigmoidHunger(tick timeSinceLastConsumption);

    public:
        Consumer(long traderId_, std::string asset_, 
            unsigned short maxPrice, tick appetiteCoef);
        Action policy(const Observation& observation) override;
        void orderPlaced(long orderId, tick now) override;
        void matchFound(const Match& match, tick now) override;
        Action lastWill(const Observation& observation) override;
};

class Producer : public Agent{
    std::string asset;
    unsigned short preferedPrice;
    unsigned int qtyPerTick = 1;

    public:
        Producer(long traderId_, std::string asset, 
            unsigned short preferedPrice);
        virtual Action policy(const Observation& observation);
};

inline long fast_sigmoid(long x) {
    return x / (1 + std::abs(x));
};
