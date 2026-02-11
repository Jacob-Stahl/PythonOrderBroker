#include "agent.h"
#include <string>
#include "tick.h"

Agent::Agent(long id) : traderId(id) {}

Action Agent::policy(const Observation& observation) {
    return Action();
}

// Consumer Implementation

unsigned short Consumer::sigmoidHunger(tick timeSinceLastConsumption){
    long sig = fast_sigmoid(timeSinceLastConsumption.raw() / ticksUntilHalfHunger.raw()); 
    return sig * maxPrice;
};

unsigned short Consumer::newLimitPrice(tick now){       
    tick timeSinceLastConsumption(0);
    if (lastConsumed.raw() > 0 && now.raw() > lastConsumed.raw()) {
         timeSinceLastConsumption = tick(now.raw() - lastConsumed.raw());
    }
    
    return sigmoidHunger(timeSinceLastConsumption);
};

Consumer::Consumer(long traderId_, std::string asset_, unsigned short maxPrice_, 
    tick appetiteCoef_): 
    Agent(traderId_), 
    lastConsumed(tick(0)), 
    maxPrice(maxPrice_), 
    ticksUntilHalfHunger(appetiteCoef_),
    asset(asset_),
    lastPlacedOrderId(0)
{}

Action Consumer::policy(const Observation& observation){
    // Don't start hungery
    if(lastConsumed.raw() == 0){
        lastConsumed = observation.time;
    }
    
    auto price = newLimitPrice(observation.time);

    // qty always set to 1 to avoid partial fills
    Order order(asset, BUY, LIMIT, price, 1);
    
    if (lastPlacedOrderId > 0) {
        return Action{order, lastPlacedOrderId};
    } else {
        return Action{order};
    }
}

void Consumer::orderPlaced(long orderId, tick now){
    lastPlacedOrderId = orderId;
}

void Consumer::matchFound(const Match& match, tick now){
    lastConsumed = now;
}

Action Consumer::lastWill(const Observation& observation){
    return Action(lastPlacedOrderId); // Cancel order before death
}


// Producer Implementation

Producer::Producer(long traderId_, std::string asset_, unsigned short preferedPrice_):
    Agent(traderId_),
    asset(asset_),
    preferedPrice(preferedPrice_)
{}

Action Producer::policy(const Observation& observation) {
    auto it = observation.assetSpreads.find(asset);

    // If asset spread is missing, trust a new orderbook is created for new asset
    Spread assetSpread = Spread();
    if (it != observation.assetSpreads.end()) {
        assetSpread = it->second;
    }

    // Cease production if there are no bids
    if(assetSpread.bidsMissing){
        return Action();
    }

    if(assetSpread.highestBid > preferedPrice){
        ++qtyPerTick;
    }
    else if(assetSpread.highestBid < preferedPrice){
        if (qtyPerTick > 0)
            --qtyPerTick;
    }

    Order order(asset, SELL, MARKET, 0, qtyPerTick);
    return Action{order};
}

