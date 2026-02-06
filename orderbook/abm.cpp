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
            agent->orderCanceled(action.doomedOrderId, tickCounter);
        };

        if(action.placeOrder){
            Order order{action.order};
            addMatcherIfNeeded(order.asset);
            orderMatchers.at(order.asset).addOrder(order);
            
            if(notifier.placedOrders.back().ordId == order.ordId){
                notifier.placedOrders.pop_back();
                agent->orderPlaced(order.ordId, tickCounter);
            }
            else // I'm trusting if an order is not placed, it MUST be in placementFailedOrders
            {
                notifier.placementFailedOrders.pop_back();
                agent->orderCanceled(order.ordId, tickCounter);
            }
        }
    };

    // TODO route match notifications. 
    // if agents and matchs are sorted by traderId, it makes this easier
};