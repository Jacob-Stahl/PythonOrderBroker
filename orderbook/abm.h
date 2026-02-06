# pragma once

#include <memory>
#include <unordered_map>
#include "matcher.h"
#include "agent.h"

class ABM{
    std::vector<std::unique_ptr<Agent>> agents;
    unsigned long tickCounter = 0;

    /// @brief Asset - Matcher
    std::unordered_map<std::string, Matcher> orderMatchers;

    public:

        ABM() = default;

        void simLoop();

};