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
   for(int i = 0; i < marketOrders.size(); i++){
        auto order = marketOrders[i];
        long int marketPrice;
        Spread spread = getSpread();
        
        // Leave this order alone, and move to the next if it shouldn't be treated as a market order
        if (!order.treatAsMarket(spread)){
            continue;
        };

        // Now we try to match this order
        matchOrder(order);
   }
};

void Matcher::matchOrder(const Order& order){ // Should this return a match, or send a notification?
    switch(order.side){
        case(BUY) : {
            // Iterate through price list
            // for each price, look at the buckets for matches
        }
        case(SELL) : {

        }
    }
}