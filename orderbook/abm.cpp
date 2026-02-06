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
};

void ABM::routeMatches(std::vector<std::unique_ptr<Agent>>& agents,
        std::vector<Match>& matches){
    
    // TODO: This sort should be moved so its not repeated constantly. But where?
    std::sort(agents.begin(), agents.end(), 
        [](const std::unique_ptr<Agent>& a,
        const std::unique_ptr<Agent>& b)
        {return a->traderId < b->traderId; });

    // Sort by buyers first
    std::sort(matches.begin(), matches.end(),
            [](const Match& a, const Match& b)
            {return a.buyer.traderId < b.buyer.traderId; });

    // TODO:
    // Route matches to buyers
    // Sort by sellers.
    // Route matches to sellers
    
    
    // Empty matches.
    matches.clear();
};

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

            // Don't match until the last agent
            orderMatchers.at(order.asset).addOrder(order, agent == agents.back());
            
            if(notifier.placedOrders.back().ordId == order.ordId){
                notifier.placedOrders.pop_back();
                agent->orderPlaced(order.ordId, tickCounter);
            }
            else // I'm trusting if an order is not placed, it MUST be in placementFailedOrders
            {
                notifier.placementFailedOrders.pop_back();
                // TODO: notify placement failed?
            }
        }
    };

    routeMatches(agents, notifier.matches);
};