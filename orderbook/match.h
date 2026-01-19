#pragma once
#include <vector>
#include "order.h"
#include <stdexcept>

// TODO time of match might be important for message ordering down stream
struct Match{
    Order buyer;
    Order seller;
    long int qty;

    public:
        Match(const Order& ord1, const Order& ord2, long int qty)
        {
            if (ord1.side == ord2.side) { std::logic_error("Can't match orders on the same side!"); }
            if (ord1.side == BUY){
                this->buyer = ord1;
                this->seller = ord2;
            }
            else{
                this->buyer = ord2;
                this->seller = ord1;
            }

            this->qty = qty;
        }
};
