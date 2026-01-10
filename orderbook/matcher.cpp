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

    // Prevents old orders from being added after new ones.
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
    for(int i = 0; i < marketOrders.size(); i++){
        Order& order = marketOrders[i];
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
            marketOrdersToRemove.insert(i);
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

        // Iterate through orders, oldest to newest
        int ordIdx = 0;
        for(Order& limitOrd : sellLimits[price]){
            if(!limitOrd.treatAsLimit(updatedSpread)){
                continue;
            }
            
            auto typeFilled = matchOrders(marketOrd, limitOrd);

            if (typeFilled == MARKET){
                goto cleanUpAndExit;
            }
            else{ // Limit filled
                limitsToRemove[price].insert(ordIdx);
            }
            ordIdx++;
        };
    }
    cleanUpAndExit:
    removeLimits(SELL, limitsToRemove);
    return marketOrderFilled;
}

bool Matcher::tryFillSellMarket(Order& marketOrd, Spread& initialSpread){
    // price and order index to remove in sell limits. sellLimits[price][order idx]
    std::map<long int, std::set<int>> limitsToRemove;
    bool marketOrderFilled = false;
    Spread updatedSpread = initialSpread;

    // Iterate through buy limit price buckets, highest to lowest
    for (auto it = buyPrices.rbegin(); it != buyPrices.rend(); ++it){
        long int price = *it;
        updatedSpread.lowestAsk = price;

        // Iterate through orders, oldest to newest
        int ordIdx = 0;
        
        for(Order& limitOrd : buyLimits[price]){
            if(!limitOrd.treatAsLimit(updatedSpread)){
                continue;
            }

            auto typeFilled = matchOrders(marketOrd, limitOrd);

            if (typeFilled == MARKET){
                goto cleanUpAndExit;
            }
            else{ // Limit filled
                limitsToRemove[price].insert(ordIdx);
            }
            ordIdx++;
        }

        cleanUpAndExit:

        removeLimits(BUY, limitsToRemove);
        return marketOrderFilled; 
    }
    return false;
}


OrdType Matcher::matchOrders(Order& marketOrd, Order& limitOrd){
    long int limUnFill = limitOrd.unfilled();
    long int markUnFill = marketOrd.unfilled();
    long int fillThisMatch = 0;

    // Limit order can be completely filled
    if(limUnFill <= markUnFill)
    {
        fillThisMatch = limUnFill;
        limitOrd.fill = limitOrd.qty;
        marketOrd.fill = marketOrd.fill + fillThisMatch;
    }
    // Market order can be completely filled
    else if (limUnFill >= markUnFill)
    {
        fillThisMatch = markUnFill;
        limitOrd.fill = limitOrd.fill + fillThisMatch;
        marketOrd.fill = marketOrd.qty;
    }
    else{
        throw std::logic_error("Could not match market and limit order!");
    }

    Match match = Match(marketOrd, limitOrd, fillThisMatch);
    this->notifier->notifyOrderMatched(match);

}

void Matcher::removeLimits(Side side, std::map<long int, std::set<int>>& limitsToRemove){ 
    for(auto priceBucket : limitsToRemove){
        int price = priceBucket.first;
        switch(side){
            case BUY : {
                removeIdxs(buyLimits[price], limitsToRemove[price]);
                
                // If there are no order at this price, remove them from the set of prices
                if (buyLimits[price].size() == 0){
                    buyPrices.erase(price);
                }

                break;
            }
            case SELL : {
                removeIdxs(sellLimits[price], limitsToRemove[price]);

                // If there are no order at this price, remove them from the set of prices
                if(sellLimits[price].size() == 0){
                    sellPrices.erase(price);
                }

                break;
            };
        }
    }
}

/// @brief Remove provided elements from an Order vec
/// @param vec 
/// @param idxToRemove 
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