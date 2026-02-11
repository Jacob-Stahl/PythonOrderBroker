#pragma once

#include "order.h"
#include "match.h"
#include "notifier.h"
#include <vector>
#include <set>
#include <queue>
#include <map>
#include <stdexcept>
#include <unordered_map>

struct PriceBin{
    unsigned short price = 0;
    unsigned int totalQty = 0;
};

struct Depth{
    std::vector<PriceBin> bidBins;
    std::vector<PriceBin> askBins;
};

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
        unsigned long lastOrdNum = 0;
        
        // TODO: Research tree balancing and its effect on performance here
        //Order FIFO queues for different prices
        std::map<unsigned short, std::vector<Order>> sellLimits;
        std::map<unsigned short, std::vector<Order>> buyLimits;

        std::vector<Order> marketOrders;
        std::set<long> canceledOrderIds;

        bool validateOrder(const Order& order);

        bool isCanceled(long ordId);

        void pushBackLimitOrder(const Order& order);

        /// @brief Try to find matches for all orders on the book
        void matchOrders();

        /// @brief Tries to fill a buy market order as much as possible. Updates fill properties in matched orders. Spread is also updated
        /// @param order 
        /// @param spread
        /// @return true if filled completely
        bool tryFillBuyMarket(Order& order, Spread& spread);

        /// @brief Tries to fill a sell market order as much as possible. Updates fill properties in matched orders.  Spread is also updated
        /// @param order 
        /// @param spread
        /// @return true if filled completely
        bool tryFillSellMarket(Order& order, Spread& spread);

        /// @brief Remove limit orders from book at given price
        /// @param limitPricesToRemove 
        /// @param side 
        void removeLimitsByPrice(std::vector<unsigned short> limitPricesToRemove, Side side);

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
        void addOrder(Order& order, bool thenMatch = true);

        void cancelOrder(long ordId);
        
        /// @brief Add all orders in the book to a vector provided by reference. They are NOT sorted by time.
        /// @param orders 
        void dumpOrdersTo(std::vector<Order>& orders);

        const Spread getSpread();
        const Depth getDepth();
        const std::unordered_map<OrdType, int> getOrderCounts();
};