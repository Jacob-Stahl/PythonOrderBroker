#pragma once

#include "order.h"
#include "match.h"
#include <vector>

class INotifier{
    public:
        virtual void notifyOrderPlaced(const Order& order);
        virtual void notifyOrderPlacementFailed(const Order& order, std::string reason);
        virtual void notifyOrderMatched(const Match& match);
};

/// @brief Notifier used for testing. Records events in vectors that can be inspected
class MockNotifier: public INotifier{
    public:
        std::vector<Order> placedOrders;
        std::vector<Order> placementFailedOrders;
        std::vector<Match> matches;

        MockNotifier() = default;

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