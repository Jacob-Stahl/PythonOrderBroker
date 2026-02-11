#include "abm.h"
#include "utils.h"

void ABM::observe(){
    latestObservation.time = tickCounter;
    for(auto& it : orderMatchers){
        latestObservation.assetSpreads[it.first] = it.second.getSpread();
        latestObservation.assetOrderDepths[it.first] = it.second.getDepth();
    };
};

void ABM::addMatcherIfNeeded(const std::string& asset){
    if(orderMatchers.find(asset) == orderMatchers.end()){
        orderMatchers.emplace(asset, Matcher(&notifier));
    }
};

void ABM::routeMatches(std::vector<Match>& matches){
    
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
        while(agentIdx < agents.size() && agents[agentIdx]->traderId < match.buyer.traderId){
            ++agentIdx;
        }
        if (agentIdx < agents.size() && agents[agentIdx]->traderId == match.buyer.traderId) {
            agents[agentIdx]->matchFound(match, tickCounter);
        }
    }

    // Sort by sellers.
    std::sort(matches.begin(), matches.end(),
        [](const Match& a, const Match& b)
        {return a.seller.traderId < b.seller.traderId; });

    // Route matches to sellers
    agentIdx = 0;
    for(auto& match : matches){
        while(agentIdx < agents.size() && agents[agentIdx]->traderId < match.seller.traderId){
            ++agentIdx;
        }
        if (agentIdx < agents.size() && agents[agentIdx]->traderId == match.seller.traderId) {
            agents[agentIdx]->matchFound(match, tickCounter);
        }
    }
    
    matches.clear();
};

void ABM::cancelOrderWithAllMatchers(long doomedOrderId){
    for(auto& it : orderMatchers){
        it.second.cancelOrder(doomedOrderId);
    };
}

void ABM::simStep(){
    // update latest observation
    observe();

    // Execute actions for all agents
    for(auto& agent: agents){
        auto action = agent->policy(latestObservation);
        
        if(action.cancelOrder){
            cancelOrderWithAllMatchers(action.doomedOrderId);
            agent->orderCanceled(action.doomedOrderId, tickCounter);
        };

        if(action.placeOrder){
            Order order{action.order};
            order.ordId = ++nextOrderId;
            addMatcherIfNeeded(order.asset);

            // TODO: delay matching until all orders are added?
            orderMatchers.at(order.asset).addOrder(order);
            
            if(!notifier.placedOrders.empty() && notifier.placedOrders.back().ordId == order.ordId){
                notifier.placedOrders.pop_back();
                agent->orderPlaced(order.ordId, tickCounter);
            }
            else if(!notifier.placementFailedOrders.empty() && notifier.placementFailedOrders.back().ordId == order.ordId)
            {
                notifier.placementFailedOrders.pop_back();
                // TODO: notify placement failed?
            }
        }
    };

    routeMatches(notifier.matches);
    ++tickCounter;

    // observe again to keep latestObservation up to date.
    // TODO: fix this double work
    observe();
};

long ABM::addAgent(std::unique_ptr<Agent> agent){
    long id = nextTraderId++;
    agent->traderId = id;
    agents.push_back(std::move(agent));
    return id;
}

void ABM::removeAgents(AgentSelector& agentSelector){
    std::vector<size_t> agentsToRemove{};
    size_t numAgents = agents.size();
    agentsToRemove.reserve(numAgents);

    for(size_t i = 0; i < numAgents; ++i){
        auto& agent = agents[i];
        if(!agentSelector.keepThis(agent)){

            // Carry out final will
            auto finalAction = agent->lastWill(latestObservation);
            if(finalAction.cancelOrder){
                cancelOrderWithAllMatchers(finalAction.doomedOrderId);
            }
            // TODO: Order placements after death not enforceable yet. fine for now

            agentsToRemove.push_back(i);
        }
    }

    // Out to pasture
    removeIdxs<std::unique_ptr<Agent>>(agents, agentsToRemove);
}