#pragma once

#include "order.h"
#include <vector>
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
        std::map<long int, std::queue<Order>> sellBook;
        std::map<long int, std::queue<Order>> buyBook;

        void matchOrders();

    public:

        /// @brief Add order to the book
        /// @param order 
        void addOrder(const Order& order);

};