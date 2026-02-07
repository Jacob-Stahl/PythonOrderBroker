# pragma once

#include <memory>
#include <unordered_map>
#include "matcher.h"
#include "agent.h"


class AgentSelector{
    public:
        virtual bool keepThis(const std::unique_ptr<Agent>& agent){return true; };
};

class ABM{
    std::vector<std::unique_ptr<Agent>> agents;
    tick tickCounter{0};

    /// @brief Asset - Matcher
    std::unordered_map<std::string, Matcher> orderMatchers;
    InMemoryNotifier notifier{};

    Observation latestObservation;

    void cancelOrderWithAllMatchers(long doomedOrderId);
    void addMatcherIfNeeded(const std::string& asset);
    void routeMatches(std::vector<Match>& matches);
    void observe();

    public:
        ABM() = default;
        void simStep();
        void removeAgents(AgentSelector& agentSelector);

};