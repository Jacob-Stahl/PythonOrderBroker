#pragma once

#include "matcher.h"
#include "match.h"
#include <string>
#include <functional>
#include <unordered_map>
#include "tick.h"

struct Observation{
    tick time;

    /// @brief asset - Spread
    std::unordered_map<std::string, Spread> assetSpreads;
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
};

class Agent{
    private:
        long traderId;

    public:
        Agent(long);

        virtual Action policy(const Observation& observation);

        virtual void matchFound(const Match& match, tick now){};
        virtual void orderPlaced(long orderId, tick now){};
        virtual void orderCanceled(long orderId, tick now){};
};

class Consumer : public Agent{

    private:
        std::string asset;
        long lastConsumed;

        unsigned short newLimitPrice(tick now);
        unsigned short sigmoidHunger(tick timeSinceLastConsumption);

    public:
        Consumer(unsigned short maxPrice, tick appetiteCoef, std::string asset);
        Action policy(const Observation& Observation) override;
};

class Producer : public Agent{
    std::string asset;
    unsigned short preferedPrice;
    unsigned int qtyPerTick = 1;

    public:
        Producer(unsigned short preferedPrice, std::string asset);
        virtual Action policy(const Observation& observation);
};

long fast_sigmoid(long x) {
    return x / (1 + std::abs(x));
};
