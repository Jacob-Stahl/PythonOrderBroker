#include "abm.h"

const Observation ABM::observe(){
    Observation o{};
    o.time = tickCounter;
    for(auto& it : orderMatchers){
        o.assetSpreads.at(it.first) = it.second.getSpread();
    };
    return o;
};

void ABM::execute(const std::vector<Action>& actions){
    // Send all cancelations first
    for(auto& action : actions){
        if(!action.cancelOrder) continue;
        for(auto& it : orderMatchers){
            it.second.cancelOrder(action.doomedOrderId);
        }
    }

    // Place all orders next
    for(auto& action : actions){
        if(!action.placeOrder) continue;
        Order order{action.order};
        addMatcherIfNeeded(order.asset);
        orderMatchers.at(order.asset).addOrder(order);
    }
}

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

    // Get Actions from Agents
    std::vector<Action> actions{};
    actions.reserve(agents.size());
    for(auto& agent: agents){
        actions.push_back(agent->policy(observation));
    }

    // Excecute all Actions
    execute(actions);

    // TODO: Route notifications back to agents.
};