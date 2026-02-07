# pragma once

#include <memory>
#include <unordered_map>
#include "matcher.h"
#include "agent.h"

class ABM{
    std::vector<std::unique_ptr<Agent>> agents;
    tick tickCounter{0};

    /// @brief Asset - Matcher
    std::unordered_map<std::string, Matcher> orderMatchers;
    InMemoryNotifier notifier{};

    Observation latestObservation;

    void addMatcherIfNeeded(const std::string& asset);
    void routeMatches(std::vector<Match>& matches);
    void observe();

    public:
        ABM() = default;
        void simStep();

};