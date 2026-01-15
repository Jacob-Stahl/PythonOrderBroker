#include "order.h"
#include "matcher.h"
#include <vector>
#include <map>
#include <stdexcept>
#include <iostream>
#include <string>
#include <string_view>
#include <format>


// TODO: handle edge cases where there are no limits on one side of the book
Spread Matcher::getSpread(){
    return Spread{*buyPrices.rbegin(), *sellPrices.rend()};
}

void Matcher::addOrder(const Order& order)
{   
    // TODO mutex that locks the book until orders are added, and matched

    // Exit early and send notifications if order is invalid
    if(!validateOrder(order)){
        return;
    }

    if(order.type == LIMIT || order.type == STOPLIMIT)
    {
        switch(order.side)
        {
            case SELL:
                this->sellLimits[order.price].push_back(order);
                this->sellPrices.insert(order.price);
                break;
            case BUY:
                this->buyLimits[order.price].push_back(order);
                this->buyPrices.insert(order.price);
                break;
        }
    }
    else if (order.type == MARKET || order.type == STOP)
    {
        marketOrders.push_back(order);
    }
    lastOrderTimestamp = order.timestamp;

    this->notifier->notifyOrderPlaced(order);
    matchOrders();
};

bool Matcher::validateOrder(const Order& order){

    // Prevent old orders from being added after new ones.
    if(order.timestamp < lastOrderTimestamp)
    {
        this->notifier->notifyOrderPlacementFailed(order, 
            "Can't add order with a timestamp older than the last added order");
        return false;
    }

    // Prevent irrational stop limit orders from being added to the book
    if(order.type == STOPLIMIT)
    {
        switch(order.side)
        {
            case SELL:
                if(order.stopPrice < order.price){
                    this->notifier->notifyOrderPlacementFailed(order, 
                        "Stop-Limit SELL can't have a stop price below the limit price");
                    return false;
                }
                break;
            case BUY:
                if(order.stopPrice > order.price){
                    this->notifier->notifyOrderPlacementFailed(order, 
                        "Stop-Limit BUY can't have a stop price above the limit price");
                    return false;
                }
                break;
        }
    }

    return true;
}

void Matcher::matchOrders()
{
    std::set<int> marketOrdersToRemove{};
    for(int ordIdx = 0; ordIdx < marketOrders.size(); ordIdx++){
        Order& order = marketOrders[ordIdx];
        long int marketPrice;
        Spread spread = getSpread();
        
        // Leave this order alone, and move to the next if it shouldn't be treated as a market order
        if (!order.treatAsMarket(spread)){
            continue;
        };

        // Now we try to match this order
        bool filled = false;

        switch(order.side){
            case(BUY) : {
                filled = tryFillBuyMarket(order, spread);
                break;
            }
            case(SELL) : {
                filled = tryFillSellMarket(order, spread);
                break;
            }
        }
        
        if(filled){
            marketOrdersToRemove.insert(ordIdx);
        }
    }

    removeIdxs(marketOrders, marketOrdersToRemove);
};

bool Matcher::tryFillBuyMarket(Order& marketOrd, Spread& initialSpread){
    // price and order index to remove in sell limits. sellLimits[price][order idx]
    std::map<long int, std::set<int>> limitsToRemove;
    bool marketOrderFilled = false;
    Spread updatedSpread = initialSpread;

    // Iterate through sell limit price buckets, lowest to highest
    for (long int price : sellPrices){
        updatedSpread.lowestAsk = price;

        marketOrderFilled = matchLimits(marketOrd, updatedSpread, sellLimits[price]);
        if(marketOrderFilled){
            return true;
        }
    }
    return false;
}

bool Matcher::tryFillSellMarket(Order& marketOrd, Spread& initialSpread){
    // price and order index to remove in sell limits. sellLimits[price][order idx]
    std::map<long int, std::set<int>> limitsToRemove;
    bool marketOrderFilled = false;
    Spread updatedSpread = initialSpread;

    // Iterate through buy limit price buckets, highest to lowest
    for (auto it = buyPrices.rbegin(); it != buyPrices.rend(); ++it){
        long int price = *it;
        updatedSpread.highestBid = price;

        marketOrderFilled = matchLimits(marketOrd, updatedSpread, buyLimits[price]);
        if(marketOrderFilled){
            return true;
        }
    }
    return false;
}

bool Matcher::matchLimits(Order& marketOrd, const Spread& spread, 
    std::vector<Order>& limitOrds){ 
    std::set<int> limitsToRemove;
    bool marketOrdFilled = false;

    for(int ordIdx = 0; ordIdx < limitOrds.size(); ordIdx++){
        if(!limitOrds[ordIdx].treatAsLimit(spread)){
            continue;
        }

        auto typeFilled = matchMarketAndLimit(marketOrd, limitOrds[ordIdx]);
        
        if (typeFilled.limit){
            limitsToRemove.insert(ordIdx);
        }
        
        if (typeFilled.market){
            removeIdxs(limitOrds, limitsToRemove);
            marketOrdFilled = true;
            break;
        }
    }

    removeIdxs(limitOrds, limitsToRemove);
    return marketOrdFilled;
}

TypeFilled Matcher::matchMarketAndLimit(Order& marketOrd, Order& limitOrd){
    long int limUnFill = limitOrd.unfilled();
    long int markUnFill = marketOrd.unfilled();
    long int fillThisMatch = 0;
    TypeFilled typeFilled = TypeFilled();

    // Limit order can be completely filled
    if(limUnFill < markUnFill)
    {
        fillThisMatch = limUnFill;
        limitOrd.fill = limitOrd.qty;
        marketOrd.fill = marketOrd.fill + fillThisMatch;
        typeFilled.limit = true;
    }
    // Market order can be completely filled
    else if (limUnFill > markUnFill)
    {
        fillThisMatch = markUnFill;
        limitOrd.fill = limitOrd.fill + fillThisMatch;
        marketOrd.fill = marketOrd.qty;
        typeFilled.market = true;
    }
    // Market and Limit have the same unfilled qty; both can be filled
    else if (limUnFill == markUnFill){
        fillThisMatch = markUnFill;
        limitOrd.fill = limitOrd.qty;
        marketOrd.fill = marketOrd.qty;
        typeFilled.both();
    }

    Match match = Match(marketOrd, limitOrd, fillThisMatch);
    this->notifier->notifyOrderMatched(match);
    return typeFilled;
}


void removeIdxs(std::vector<Order>& orders, const std::set<int>& idxToRemove){
    
    // elements to keep are moved from the read iterator to the write iterator.
    size_t write = 0;
    for (size_t read = 0; read < orders.size(); ++read) {
        if (idxToRemove.find(read) == idxToRemove.end()) {
            orders[write++] = std::move(orders[read]);
        }
    }

    // at this point, there is a tail of garbage at the end of the vector.
    // resize to remove it.
    orders.resize(write);
}