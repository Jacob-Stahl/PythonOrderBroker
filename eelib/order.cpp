
#include <string>
#include "order.h"

const unsigned int Order::amt(){
    return qty * price;
}

const bool Order::treatAsMarket(const Spread& spread){
    switch(type){
        case MARKET:
            return true;
        case LIMIT:
            return false;
        case STOPLIMIT:
            return false;
        case STOP : {

            /*
            https://www.onixs.biz/fix-dictionary/4.4/glossary.html#Stop:~:text=Stop-,A%20stop%20order%20to%20buy%20which%20becomes%20a%20market%20order%20when,stop%20price%20after%20the%20order%20is%20represented%20in%20the%20Trading%20Crowd.,-OrdType%20%3C40%3E      
            Treat buy-stop as a buy-market if marketPrice >= price
            Treat sell-stop as a sell-market if marketPrice <= price
            */
            if (side == BUY) {
                if (spread.asksMissing) return false;
                return spread.lowestAsk >= stopPrice;
            } else {
                if (spread.bidsMissing) return false;
                return spread.highestBid <= stopPrice;
            }
        }
    }
}

const bool Order::treatAsLimit(const Spread& spread){
    switch(type){
        case MARKET:
            return false;
        case LIMIT:
            return true;
        case STOPLIMIT:
            if(side == BUY){
                if (spread.asksMissing) return false;
                return spread.lowestAsk >= stopPrice;
            } else {
                if (spread.bidsMissing) return false;
                return spread.highestBid <= stopPrice;
            }
        case STOP : {
            return false;
        }
    }
}