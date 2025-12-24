#include "order.h"
#include "matcher.h"
#include <vector>
#include <map>
#include <stdexcept>


void Matcher::addOrder(const Order& order)
{   
    // TODO mutex that locks the book until orders are added

    // Prevents old orders from being added after new ones.
    if(order.timestamp < lastOrderTimestamp)
    {
        throw new std::invalid_argument("Can't add order with a timestamp older than the last added order");
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
    // TODO Added-To-Orderbook callback here?

    // TODO unlock mutex here? call match async?

    matchOrders(lastOrderTimestamp);
};

Spread Matcher::getSpread(){
    return Spread{*buyPrices.rbegin(), *sellPrices.rend()};
}

void Matcher::matchOrders(int lastOrderTimestamp)
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

        

   }
};