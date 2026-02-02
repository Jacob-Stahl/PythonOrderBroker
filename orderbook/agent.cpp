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

        Consumer(unsigned short, long);

        virtual Action policy(const Observation& Observation){
            
        }
};

unsigned short hunger(long timeSinceLastConsumption, unsigned short maxPrice, long appetiteCoef){
    long sig = fast_sigmoid(timeSinceLastConsumption / appetiteCoef);
    return sig * maxPrice;
};