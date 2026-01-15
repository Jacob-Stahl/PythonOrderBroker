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
            if (buyer.side != BUY) { throw std::logic_error("buyer must have side BUY!");}
            if (seller.side != SELL) { throw std::logic_error("seller must have side SELL!");}

            this->buyer = buyer;
            this->seller = seller;
            this->qty = qty;
        }

        // TODO use proper getters
        long int getQty() {return qty;}
};
