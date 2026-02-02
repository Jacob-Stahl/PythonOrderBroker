#include <agent.h>

class Consumer : public Agent{
    private:
        bool hasBidBefore = false;
        long lastConsumedTimestamp;
        unsigned short maxPrice;
        long appetiteCoef;

        unsigned short newLimitPrice(long currentTime){       
            long timeSinceLastConsumption = lastConsumedTimestamp - currentTime;
            return hunger(timeSinceLastConsumption, maxPrice, appetiteCoef);
        };

    public:
        Consumer::Consumer(unsigned short maxPrice_, long appetiteCoef_)
            : lastConsumedTimestamp(0), maxPrice(maxPrice_), appetiteCoef(appetiteCoef_)
        {}
        virtual Action policy(const Observation& Observation){
            // Cancel old order
            // Update limit price
        }
};

unsigned short hunger(
    long timeSinceLastConsumption, 
    unsigned short maxPrice, 
    long appetiteCoef){
    long sig = fast_sigmoid(timeSinceLastConsumption / appetiteCoef);
    return sig * maxPrice;
};