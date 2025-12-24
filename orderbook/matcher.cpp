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
                break;
            case BUY:
                this->buyLimits[order.price].push(order);
                break;
        }
    }
    // market and stops get added to buy and sell queues
    else if (order.type == MARKET || order.type == STOP)
    {
        switch(order.side)
        {
            case SELL:
                this->buyQueue.push(order);
                break;
            case BUY:
                this->sellQueue.push(order);
                break;
        }
    }

    lastOrderTimestamp = order.timestamp;
    // TODO Added-To-Orderbook callback here?

    // TODO unlock mutex here? call match async?

    matchOrders(lastOrderTimestamp);
};

void Matcher::matchOrders(int lastOrderTimestamp)
{
    
};