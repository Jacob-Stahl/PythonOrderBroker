#include "order.h"
#include "matcher.h"
#include <vector>
#include <map>
#include <stdexcept>
#include <iostream>
#include <string>
#include <string_view>
#include <format>

Spread Matcher::getSpread(){
    return Spread{*buyPrices.rbegin(), *sellPrices.rend()};
}

void Matcher::addOrder(const Order& order)
{   
    // TODO mutex that locks the book until orders are added, and matched

    // Prevents old orders from being added after new ones.
    if(order.timestamp < lastOrderTimestamp)
    {
        this->notifier.notifyOrderPlacementFailed(order, "Can't add order with a timestamp older than the last added order");
    }

    // Add limit orders to the book
    if(order.type == LIMIT)
    {
        switch(order.side)
        {
            case SELL:
                this->sellLimits[order.price].push_back(order);
                this->sellPrices.insert(order.price);
                break;
            case BUY:
                this->buyLimits[order.price].push_back(order);
                this->buyPrices.insert(order.price);
                break;
        }
    }
    else if (order.type == MARKET || order.type == STOP)
    {
        marketOrders.push_back(order);
    }
    lastOrderTimestamp = order.timestamp;

    this->notifier.notifyOrderPlaced(order);
    matchOrders();
};

void Matcher::matchOrders()
{
    std::set<int> marketOrdersToRemove{};
    for(int i = 0; i < marketOrders.size(); i++){
        auto order = marketOrders[i];
        long int marketPrice;
        Spread spread = getSpread();
        
        // Leave this order alone, and move to the next if it shouldn't be treated as a market order
        if (!order.treatAsMarket(spread)){
            continue;
        };

        // Now we try to match this order
        bool filled = false;

        switch(order.side){
            case(BUY) : {
                filled = tryFillBuyMarket(order);
            }
            case(SELL) : {
                filled = tryFillSellMarket(order);
            }
        }
        

        if(filled){
            marketOrdersToRemove.insert(i);
        }
    }

    removeIdxs(marketOrders, marketOrdersToRemove);
};

bool Matcher::tryFillBuyMarket(Order& order){
    // sellLimits[price][order idx]
    std::map<int, std::set<int>> limitsToRemove;

    bool marketOrderFilled = false;

    // Iterate through sell limit price buckets, lowest to highest
    for (auto it = sellPrices.rbegin(); it != sellPrices.rend(); ++it){
        int price = *it;

        // Iterate through orders, oldest to newest
        int ordIdx = 0;
        for(Order& limitOrd : sellLimits[price]){
            long int limUnFill = limitOrd.unfilled();
            long int markUnFill = order.unfilled();

            // Limit order can be completely filled
            if(limUnFill <= markUnFill)
            {
                long int fillThisMatch = limUnFill;

                // TODO: does this update the order in the book?
                // Update fills
                limitOrd.fill = limitOrd.qty;
                order.fill = order.fill + fillThisMatch;

                // TODO: make sure creating a match like this doesn't 
                // create problems when
                // we make notifier async
                Match match = Match(order, limitOrd, fillThisMatch);
                
                notifier.notifyOrderMatched(match);
                limitsToRemove[price].insert(ordIdx);
            }
            // Market order can be comp
            else if (limUnFill >= markUnFill)
            {
                long int fillThisMatch = markUnFill;

                limitOrd.fill = limitOrd.fill + fillThisMatch;
                order.fill = order.qty;

                Match match = Match(order, limitOrd, fillThisMatch);

                notifier.notifyOrderMatched(match);
                
                // Limit isn't completely filled, leave it in the book
                marketOrderFilled = true;
            }

            if (marketOrderFilled){
                goto marketOrderFilled;
            }
            ordIdx++;
        };
    }

    marketOrderFilled:

    // TODO: is it dangerous to clear matched limit after sending notifications?
    removeLimits(SELL, limitsToRemove);

    return marketOrderFilled;
}

bool Matcher::tryFillSellMarket(Order& order){
    for (int price : buyPrices){
        //std::vector<Order> orderQueue = buyLimits[price];
    }


    return false;
}

void Matcher::removeLimits(Side side, std::map<int, std::set<int>>& limitsToRemove){ 
    for(auto priceBucket : limitsToRemove){
        int price = priceBucket.first;
        switch(side){
            case BUY : removeIdxs(buyLimits[price], limitsToRemove[price]);
            case SELL : removeIdxs(sellLimits[price], limitsToRemove[price]);
        }
    }
}

/// @brief Remove provided elements from an Order vec
/// @param vec 
/// @param idxToRemove 
void removeIdxs(std::vector<Order>& vec, std::set<int>& idxToRemove){
    
    // shift orders to keep over the ones to be removed.
    size_t write = 0;
    for (size_t read = 0; read < vec.size(); ++read) {
        if (!(idxToRemove.find(read) == idxToRemove.end())) {
            vec[write++] = std::move(vec[read]);
        }
    }

    // at this point, there is a tail of garbage at the end of the vector.
    // resize to remove it.
    vec.resize(write);
}