#pragma once

#include "order.h"
#include "match.h"
#include "notifier.h"
#include <vector>
#include <set>
#include <queue>
#include <map>
#include <stdexcept>


/// @brief Processes orders for a single symbol
class Matcher{

    private:
        long int lastOrderTimestamp = 0;

        //Order FIFO queues for different prices
        std::map<long int, std::vector<Order>> sellLimits;
        std::map<long int, std::vector<Order>> buyLimits;

        // Ordered set of prices
        std::set<long int> sellPrices;
        std::set<long int> buyPrices;

        // Market AND stop orders. TODO find a better name for this
        std::vector<Order> marketOrders;
        
        INotifier notifier = MockNotifier();

        bool validateOrder(const Order& order);

        /// @brief Try to find matches for all orders on the book
        /// @param lastOrderTimestamp 
        void matchOrders();

        /// @brief Tries to fill a buy market order as much as possible. Updates fill properties in matched orders
        /// @param order 
        /// @return true if filled completely
        bool tryFillBuyMarket(Order& order);

        /// @brief Tries to fill a sell market order as much as possible. Updates fill properties in matched orders
        /// @param order 
        /// @return true if filled completely
        bool tryFillSellMarket(Order& order);

    public:

        Matcher() = default;

        Matcher(INotifier& notifier){
            Matcher();
            this->notifier = notifier;
        }

        /// @brief Add order to the book
        /// @param order 
        void addOrder(const Order& order);

        Spread getSpread();

        void removeLimits(
            Side side,
            std::map<int, std::set<int>>& limitsToRemove);
};


void removeIdxs(std::vector<Order>& orderVec, std::set<int>& idxToRemove);