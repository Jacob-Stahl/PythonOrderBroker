#include <agent.h>
#include <string>
#include "tick.h"

class Consumer : public Agent{
    private:
        std::string asset;
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
        Consumer(long traderId_, std::string asset_, unsigned short maxPrice_, 
            long appetiteCoef_): 
            Agent(traderId_), 
            lastConsumed(0), 
            maxPrice(maxPrice_), 
            ticksUntilHalfHunger(appetiteCoef_),
            asset(asset_)
        {}
        virtual Action policy(const Observation& observation){

            // Don't start hungery
            if(lastConsumed == tick(0)){
                lastConsumed = observation.time;
            }
            
            auto price = newLimitPrice(observation.time);

            // qty always set to 1 to avoid partial fills
            Order order(asset, BUY, LIMIT, price, 1);          
            return Action{order, lastPlacedOrderId};

        }

        virtual void orderPlaced(long orderId, tick now) override{
            lastPlacedOrderId = orderId;
        }

        virtual void matchFound(const Match& match, tick now){
            lastConsumed = now;
        }

        virtual Action finalWill(const Observation& observation){
            return Action(lastPlacedOrderId); // Cancel order before death
        }
};

class Producer : public Agent{
    std::string asset;
    unsigned short preferedPrice;
    unsigned int qtyPerTick = 1;

    public:
        Producer(long traderId_, std::string asset_, unsigned short preferedPrice_):
            Agent(traderId_),
            asset(asset_),
            preferedPrice(preferedPrice_)
        {}

        virtual Action policy(const Observation& observation) override {
            const Spread& assetSpread = observation.assetSpreads.find(asset)->second;

            // Cease production if there are no bids
            if(assetSpread.bidsMissing){
                return Action();
            }

            if(assetSpread.highestBid > preferedPrice){
                ++qtyPerTick;
            }
            else if(assetSpread.highestBid < preferedPrice){
                --qtyPerTick;
            }

            Order order(asset, SELL, MARKET, 0, qtyPerTick);
            return Action{order};
        }
};

