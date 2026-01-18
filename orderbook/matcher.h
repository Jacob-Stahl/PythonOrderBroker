#pragma once

#include "order.h"
#include "match.h"
#include "notifier.h"
#include <vector>
#include <set>
#include <queue>
#include <map>
#include <stdexcept>

struct TypeFilled{
    bool market = false;
    bool limit = false;

    public:
        void both(){
            market = true;
            limit = true;
        }
};

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

        std::vector<Order> marketOrders;

        bool validateOrder(const Order& order);

        void pushBackLimitOrder(const Order& order);

        /// @brief Try to find matches for all orders on the book
        /// @param lastOrderTimestamp 
        void matchOrders();

        /// @brief Tries to fill a buy market order as much as possible. Updates fill properties in matched orders
        /// @param order 
        /// @param initialSpread
        /// @return true if filled completely
        bool tryFillBuyMarket(Order& order, Spread& initialSpread);

        /// @brief Tries to fill a sell market order as much as possible. Updates fill properties in matched orders
        /// @param order 
        /// @param initialSpread
        /// @return true if filled completely
        bool tryFillSellMarket(Order& order, Spread& initialSpread);

        /// @brief Remove limit orders from book at given price
        /// @param limitPricesToRemove 
        /// @param side 
        void removeLimitsByPrice(std::vector<long int> limitPricesToRemove, Side side);

        /// @brief Matches a market order with limits sorted from the oldest to newest
        /// @param marketOrd 
        /// @param limitOrds 
        /// @return true if market order is filled
        bool matchLimits(Order& marketOrd, const Spread& spread, 
            std::vector<Order>& limitOrds);


        /// @brief Matches a market order an a limit. returns the type that was completely filled
        /// @param market 
        /// @param limit 
        /// @return 
        TypeFilled matchMarketAndLimit(Order& market, Order& limit);

        Matcher() = default;
    public:
        INotifier* notifier;

        Matcher(INotifier* notif): notifier(notif){
            Matcher();
        }

        /// @brief Add order to the book
        /// @param order 
        void addOrder(const Order& order, bool thenMatch = true);

        /// @brief Add all orders in the book to a vector provided by reference. They are NOT sorted by time.
        /// @param orders 
        void dumpOrdersTo(std::vector<Order>& orders);

        Spread getSpread();
};

/// @brief Remove provided elements from an Order vec
/// @param orders
/// @param idxToRemove 
void removeIdxs(std::vector<Order>& orders, const std::set<int>& idxToRemove);