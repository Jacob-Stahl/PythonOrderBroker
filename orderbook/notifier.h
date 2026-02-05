#pragma once

#include "order.h"
#include "match.h"
#include <vector>

class INotifier{
    public:
    virtual void notifyOrderPlaced(const Order& order) = 0;
    virtual void notifyFailedOrderPlacement(const Order& order, std::string reason) = 0;
    virtual void notifyOrderMatched(const Match& match) = 0;
};

/// @brief Notifier used for testing. Events are stored internally for inspection later
class MockNotifier: public INotifier{
    public:
        std::vector<Order> placedOrders;
        std::vector<Order> failedOrderPlacements;
        std::vector<Match> matches;

        MockNotifier() = default;

        void notifyOrderPlaced(const Order& order){
            placedOrders.push_back(order);
        }
        void notifyFailedOrderPlacement(const Order& order, std::string reason){
            failedOrderPlacements.push_back(order);
        }
        void notifyOrderMatched(const Match& match){
            matches.push_back(match);
        }
};