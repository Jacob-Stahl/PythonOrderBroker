#pragma once

#include "order.h"
#include "match.h"
#include <vector>

class INotifier{
    public:
    virtual void notifyOrderPlaced(const Order& order) = 0;
    virtual void notifyOrderPlacementFailed(const Order& order, std::string reason) = 0;
    virtual void notifyOrderMatched(const Match& match) = 0;
};

/// @brief Stores events in public vectors
class InMemoryNotifier: public INotifier{
    public:
        std::vector<Order> placedOrders;
        std::vector<Order> placementFailedOrders;
        std::vector<Match> matches;

        InMemoryNotifier() = default;

        void notifyOrderPlaced(const Order& order){
            placedOrders.push_back(order);
        }
        void notifyOrderPlacementFailed(const Order& order, std::string reason){
            placementFailedOrders.push_back(order);
        }
        void notifyOrderMatched(const Match& match){
            matches.push_back(match);
        }
};