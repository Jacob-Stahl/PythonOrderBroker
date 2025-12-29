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
                this->sellLimits[order.price].push(order);
                this->sellPrices.insert(order.price);
                break;
            case BUY:
                this->buyLimits[order.price].push(order);
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
        Match match = Match(order);
        
        switch(order.side){
            case(BUY) : {
                for (auto it = sellPrices.rbegin(); it != sellPrices.rend(); ++it){
                    int price = *it;
                    std::queue<Order> orderQueue = sellLimits[price];

                }
            }
            case(SELL) : {
                for (int price : buyPrices){
                    std::queue<Order> orderQueue = buyLimits[price];

                }
            }
        }

        if(match.isValid()){
            marketOrdersToRemove.insert(i);
        }
    }

    removeIdxs(marketOrders, marketOrdersToRemove);
};

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