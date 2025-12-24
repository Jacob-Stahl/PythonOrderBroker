#pragma once

#include "order.h"
#include <vector>
#include <set>
#include <queue>
#include <map>
#include <stdexcept>

/*
Buy/Sell
    Price (Map)
        Timestamp (FIFO Queue)

*/

/// @brief Processes orders for a single symbol
class Matcher{

    private:
        long int lastOrderTimestamp = 0;

        //Order FIFO queues for different prices
        std::map<long int, std::queue<Order>> sellLimits;
        std::map<long int, std::queue<Order>> buyLimits;

        // Ordered set of prices
        std::set<long int> sellPrices;
        std::set<long int> buyPrices;

        // Market AND stop orders. TODO find a better name for this
        std::vector<Order> marketOrders;

        void notifyOrderPlaced(const Order& order);
        void notifyOrderPlacementFailed(const Order& order, std::string reason);
        void notifyOrderMatched();

        /// @brief Try to find matches for all orders on the book
        /// @param lastOrderTimestamp 
        void matchOrders();

        /// @brief Try to match a specific order. Should ONLY pass market orders, or orders that can be treated as market
        /// @param order 
        void matchOrder(const Order& order);

    public:

        Matcher() = default;

        /// @brief Add order to the book
        /// @param order 
        void addOrder(const Order& order);

        Spread Matcher::getSpread();
};

