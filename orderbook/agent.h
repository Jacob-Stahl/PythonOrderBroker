#include "matcher.h"
#include "match.h"
#include <string>
#include <functional>
#include <unordered_map>

struct Observation{
    long timestamp;

    /// @brief asset - Spread
    std::unordered_map<std::string, Spread> assetSpreads;
};

struct Action{
    bool placeOrder;
    Order order;

    bool cancelOrder;
    long doomedOrderId;
};

class Agent{
    private:
        long traderId;

    public:
        Agent(long);

        virtual Action policy(const Observation& observation);
        virtual void matchFound(const Match& match);
        virtual void orderPlaced(long orderId);
        virtual void orderCanceled(long orderId);
};

class Consumer : public Agent{

    private:
        long lastConsumedTimestamp;

        unsigned short newLimitPrice(long currentTime);

    public:
        Consumer(unsigned short maxPrice, long appetiteCoef);
        Action policy(const Observation& Observation) override;
};

long fast_sigmoid(long x) {
    return x / (1 + std::abs(x));
};

unsigned short hunger(long timeSinceLastConsumption, unsigned short maxPrice, long appetiteCoef);