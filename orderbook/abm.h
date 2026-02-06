# pragma once

#include <memory>
#include <unordered_map>
#include "matcher.h"
#include "agent.h"

class ABM{
    std::vector<std::unique_ptr<Agent>> agents;
    tick tickCounter{0};
    bool running = true;

    /// @brief Asset - Matcher
    std::unordered_map<std::string, Matcher> orderMatchers;
    InMemoryNotifier notifier{};

    void addMatcherIfNeeded(const std::string& asset);
    void routeMatches(std::vector<std::unique_ptr<Agent>>& agents,
        std::vector<Match>& matches);
    const Observation observe();
    void simStep();

    public:
        ABM() = default;
        void simLoop();

};