#include <agent.h>

class Consumer : public Agent{
    private:
        long lastConsumedTimestamp = 0;
        long lastPlacedOrderId;

        unsigned short maxPrice;
        long appetiteCoef;

        const unsigned short newLimitPrice(long currentTime){       
            long timeSinceLastConsumption = lastConsumedTimestamp - currentTime;
            return hunger(timeSinceLastConsumption, maxPrice, appetiteCoef);
        };

    public:
        Consumer(long traderId_, unsigned short maxPrice_, long appetiteCoef_): 
            Agent(traderId_), 
            lastConsumedTimestamp(0), 
            maxPrice(maxPrice_), 
            appetiteCoef(appetiteCoef_)
        {}
        virtual Action policy(const Observation& observation){

            // Don't start hungery
            if(lastConsumedTimestamp == 0){
                lastConsumedTimestamp = observation.timestamp;
            }
            
            auto price = newLimitPrice(observation.timestamp);
            Order order(BUY, LIMIT, price, 1);          
            return Action{order, lastPlacedOrderId};

        }

        virtual void orderPlaced(long orderId) override{
            lastPlacedOrderId = orderId;
        }

        virtual void matchFound(const Match& match){
            // TODO, what do we set lastConsumedTimestamp too?
        }
};

unsigned short hunger(
    long timeSinceLastConsumption, 
    unsigned short maxPrice, 
    long appetiteCoef){
    long sig = fast_sigmoid(timeSinceLastConsumption / appetiteCoef);
    return sig * maxPrice;
};