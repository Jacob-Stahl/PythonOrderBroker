#include "order.h"
#include "matcher.h"
#include <vector>
#include <map>
#include <stdexcept>


void Matcher::addOrder(const Order& order)
{

    // Prevents old orders from being added after new ones.
    if(order.timestamp < lastOrderTimestamp)
    {
        throw new std::invalid_argument("Can't add order with a timestamp older than the last added order");
    }
    
    switch(order.side)
    {
        case SELL:
            this->sellBook[order.price].push(order);
            break;
        case BUY:
            this->buyBook[order.price].push(order);
            break;
    }

    lastOrderTimestamp = order.timestamp;
    // TODO Added-To-Orderbook callback here?


    matchOrders();

};

void Matcher::matchOrders()
{

};