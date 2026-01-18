#include "order.h"
#include "matcher.h"
#include <vector>
#include <map>
#include <stdexcept>
#include <iostream>
#include <string>
#include <string_view>
#include <format>

Spread Matcher::getSpread(){

    bool bidsMissing = buyPrices.end() == buyPrices.begin();
    bool asksMissing = sellPrices.end() == sellPrices.begin();

    long int bid;
    if(!bidsMissing){
        bid = *buyPrices.rbegin();
    }

    long int ask;
    if(!asksMissing){
        ask = *sellPrices.begin();
    }
    
    return Spread{bidsMissing, asksMissing, bid, ask};
}

void Matcher::addOrder(const Order& order)
{   
    // TODO mutex that locks the book until orders are added, and matched

    // Exit early and send notifications if order is invalid
    if(!validateOrder(order)){
        return;
    }

    switch (order.type) {
        case LIMIT:
        case STOPLIMIT:
            pushBackLimitOrder(order);
            break;
        case MARKET:
        case STOP:
            marketOrders.push_back(order);
            break;
        default:
            std::logic_error("Order type not implemented!");
    }

    lastOrderTimestamp = order.timestamp;
    this->notifier->notifyOrderPlaced(order);
    matchOrders();
};

void Matcher::pushBackLimitOrder(const Order& order){
    bool newLimitPrice;
    
    switch(order.side)
    {
        case SELL:
            newLimitPrice = sellLimits.find(order.price) == sellLimits.end();
            if(newLimitPrice){
                this->sellPrices.insert(order.price);
                this->sellLimits[order.price] = std::vector<Order>();
            }
            this->sellLimits[order.price].push_back(order);
            break;
        case BUY:
            newLimitPrice = buyLimits.find(order.price) == buyLimits.end();
            if(newLimitPrice){
                this->buyPrices.insert(order.price);
                this->buyLimits[order.price] = std::vector<Order>();
            }
            this->buyLimits[order.price].push_back(order);
            break;
    }
}

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
    if(marketOrders.size() == 0){
        return; // Exit early if there are now market orders
    }

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
            case(BUY) :
                // No sell limits on the book to match with
                if(spread.asksMissing){ break; }

                // Try to match with limits on the book
                filled = tryFillBuyMarket(order, spread);
                break;   
            case(SELL) :
                // No buy limits on the book to match with
                if(spread.bidsMissing){ break; }

                // try to match with limits on the book
                filled = tryFillSellMarket(order, spread);
                break;
        }
        
        if(filled){
            marketOrdersToRemove.insert(ordIdx);
        }
    }

    removeIdxs(marketOrders, marketOrdersToRemove);
};

bool Matcher::tryFillBuyMarket(Order& marketOrd, Spread& initialSpread){
    bool marketOrderFilled = false;
    Spread updatedSpread = initialSpread;
    std::vector<long int> limitPricesToRemove{};

    // Iterate through sell limit price buckets, lowest to highest
    for (long int price : sellPrices){
        updatedSpread.lowestAsk = price;
        marketOrderFilled = matchLimits(marketOrd, updatedSpread, sellLimits[price]);
        if(sellLimits[price].size() == 0){
            limitPricesToRemove.push_back(price);
        }
        if(marketOrderFilled){
            break;
        }
    }

    removeLimitsByPrice(limitPricesToRemove, SELL);
    return marketOrderFilled;
}

bool Matcher::tryFillSellMarket(Order& marketOrd, Spread& initialSpread){
    bool marketOrderFilled = false;
    Spread updatedSpread = initialSpread;
    std::vector<long int> limitPricesToRemove{};

    // Iterate through buy limit price buckets, highest to lowest
    for (auto it = buyPrices.rbegin(); it != buyPrices.rend(); ++it){
        long int price = *it;
        updatedSpread.highestBid = price;
        marketOrderFilled = matchLimits(marketOrd, updatedSpread, buyLimits[price]);
        if(buyLimits[price].size() == 0){
            limitPricesToRemove.push_back(price);
        }
        if(marketOrderFilled){
            break;
        }
    }

    removeLimitsByPrice(limitPricesToRemove, BUY);
    return marketOrderFilled;
}

void Matcher::removeLimitsByPrice(std::vector<long int> limitPricesToRemove, Side side){
    
    if(limitPricesToRemove.size() == 0){
        return; // early return if there are no limit prices to remove
    }
    
    switch(side){
        case SELL:
            for(auto price : limitPricesToRemove){
                if(sellLimits[price].size()){
                    throw std::logic_error("Can't remove non-empty list of limits!");
                }
                sellLimits.erase(price);
                sellPrices.erase(price);
            }
            break;
        case BUY:
            for(auto price : limitPricesToRemove){
                if(buyLimits[price].size()){
                    throw std::logic_error("Can't remove non-empty list of limits!");
                }
                buyLimits.erase(price);
                buyPrices.erase(price);
            }
            break;
    }
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
    
    if(idxToRemove.size() == 0) { return; }

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