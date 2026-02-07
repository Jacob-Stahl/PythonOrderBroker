#include "abm.h"

void ABM::observe(){
    latestObservation.time = tickCounter;
    for(auto& it : orderMatchers){
        latestObservation.assetSpreads.at(it.first) = it.second.getSpread();
        latestObservation.assetOrderDepths.at(it.first) = it.second.getDepth();
    };
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

    // Sort by buyers.
    std::sort(matches.begin(), matches.end(),
            [](const Match& a, const Match& b)
            {return a.buyer.traderId < b.buyer.traderId; });

    // Route matches to buyers
    size_t agentIdx = 0;
    for(auto& match : matches){
        while(match.buyer.traderId != agents[agentIdx]->traderId){
            ++agentIdx;
        }
        agents[agentIdx]->matchFound(match, tickCounter);
    }

    // Sort by sellers.
    std::sort(matches.begin(), matches.end(),
        [](const Match& a, const Match& b)
        {return a.seller.traderId < b.seller.traderId; });

    // Route matches to sellers
    agentIdx = 0;
    for(auto& match : matches){
        while(match.seller.traderId != agents[agentIdx]->traderId){
            ++agentIdx;
        }
        agents[agentIdx]->matchFound(match, tickCounter);
    }
    
    matches.clear();
};

void ABM::simStep(){
    // update latest observation
    observe();

    // Execute actions for all agents
    for(auto& agent: agents){
        auto action = agent->policy(latestObservation);
        
        if(action.cancelOrder){
            for(auto& it : orderMatchers){
                it.second.cancelOrder(action.doomedOrderId);
            };
            agent->orderCanceled(action.doomedOrderId, tickCounter);
        };

        if(action.placeOrder){
            Order order{action.order};
            addMatcherIfNeeded(order.asset);

            // TODO: delay matching until all orders are added?
            orderMatchers.at(order.asset).addOrder(order);
            
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
    ++tickCounter;
};