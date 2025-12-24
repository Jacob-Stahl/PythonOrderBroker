#pragma once

#include "order.h"
#include <vector>
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
        std::map<long int, std::queue<Order>> sellLimits;
        std::map<long int, std::queue<Order>> buyLimits;

        std::queue<Order> buyQueue;
        std::queue<Order> sellQueue;

        void matchOrders(int lastOrderTimestamp);

    public:

        Matcher() = default;

        /// @brief Add order to the book
        /// @param order 
        void addOrder(const Order& order);

};