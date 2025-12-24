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

        std::vector<Order> marketOrders;

        void matchOrders(int lastOrderTimestamp);

    public:

        Matcher() = default;

        /// @brief Add order to the book
        /// @param order 
        void addOrder(const Order& order);

        Spread Matcher::getSpread();
};

