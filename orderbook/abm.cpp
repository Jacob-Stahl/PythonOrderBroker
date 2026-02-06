#include "abm.h"

const Observation ABM::observe(){
    Observation o{};
    o.time = tickCounter;
    for(auto& it : orderMatchers){
        o.assetSpreads.at(it.first) = it.second.getSpread();
    };
    return o;
};


void ABM::addMatcherIfNeeded(const std::string& asset){
    if(orderMatchers.find(asset) == orderMatchers.end()){
        orderMatchers[asset] = Matcher(&notifier);
    }
}

void ABM::simLoop(){
    // Setup?

    while(running){
        simStep();
        ++tickCounter;
    }

    // Tear down? Dump results?
};

void ABM::simStep(){
    // Get Observation
    const Observation observation = observe();

    // Execute actions for all agents
    for(auto& agent: agents){
        auto action = agent->policy(observation);
        
        if(action.cancelOrder){
            for(auto& it : orderMatchers){
                it.second.cancelOrder(action.doomedOrderId);
            };
            // TODO route cancelation notification
            // At most, one cancelation should happen.
        };

        if(action.placeOrder){
            Order order{action.order};
            addMatcherIfNeeded(order.asset);
            orderMatchers.at(order.asset).addOrder(order);

            // TODO check if order was placed or failed, then route notificaitons
        }
    };
};