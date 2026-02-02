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
    std::function<void(const Match&)> matchCallback;
    bool placeOrder;
    Order order;
};

class Agent{
    public:
        virtual Action policy(const Observation& observation);
};

class Consumer : public Agent{

    private:
        long lastConsumedTimestamp;

        unsigned short newLimitPrice(long currentTime);

    public:
        Action policy(const Observation& Observation) override;
};

long fast_sigmoid(long x) {
    return x / (1 + std::abs(x));
};

unsigned short hunger(long timeSinceLastConsumption, unsigned short maxPrice, long appetiteCoef);