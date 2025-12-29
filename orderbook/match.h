#pragma once

#include <vector>
#include "order.h"

// TODO time of match might be important for message ordering down stream
struct Match{
    Order order;
    std::vector<Order> matchingOrders;

    Match(const Order& order) : order(order), matchingOrders() {}


    bool isValid();
};
