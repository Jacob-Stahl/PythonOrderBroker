#include <agent.h>
#include "tick.h"

class Consumer : public Agent{
    private:
        tick lastConsumed;
        long lastPlacedOrderId;
        unsigned short maxPrice;
        tick ticksUntilHalfHunger;

        unsigned short sigmoidHunger(tick timeSinceLastConsumption){
            long sig = fast_sigmoid(timeSinceLastConsumption / ticksUntilHalfHunger);
            return sig * maxPrice;
        };

        const unsigned short newLimitPrice(tick now){       
            tick timeSinceLastConsumption = lastConsumed - now;
            return sigmoidHunger(timeSinceLastConsumption);
        };

    public:
        Consumer(long traderId_, unsigned short maxPrice_, long appetiteCoef_): 
            Agent(traderId_), 
            lastConsumed(0), 
            maxPrice(maxPrice_), 
            ticksUntilHalfHunger(appetiteCoef_)
        {}
        virtual Action policy(const Observation& observation){

            // Don't start hungery
            if(lastConsumed == tick(0)){
                lastConsumed = observation.time;
            }
            
            auto price = newLimitPrice(observation.time);

            // qty always set to 1 to avoid partial fills
            Order order(BUY, LIMIT, price, 1);          
            return Action{order, lastPlacedOrderId};

        }

        virtual void orderPlaced(long orderId, tick now) override{
            lastPlacedOrderId = orderId;
        }

        virtual void matchFound(const Match& match, tick now){
            lastConsumed = now;
        }
};

