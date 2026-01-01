#pragma once
#include <vector>
#include "order.h"

// TODO time of match might be important for message ordering down stream
class Match{
    Order buyer;
    Order seller;
    long int qty;

    public:
        Match(const Order& buyer, const Order& seller, long int qty)
        {
            // TODO add guards order sides.

            this->buyer = buyer;
            this->seller = seller;
            this->qty = qty;
        }

        // TODO use proper getters
        long int getQty() {return qty;}
};
