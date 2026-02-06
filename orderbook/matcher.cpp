#include "order.h"
#include "matcher.h"
#include <vector>
#include <map>
#include <stdexcept>
#include <iostream>
#include <string>
#include <string_view>
#include <format>
#include <unordered_map>

// TODO: consider STOPLIMITS in spread? does this create a chicken and egg problem?
// TODO: is it worth removing canceled orders from spread?
const Spread Matcher::getSpread(){
    bool bidsMissing = true;
    bool asksMissing = true;

    unsigned short bid;
    for (auto it = buyLimits.rbegin(); it != buyLimits.rend(); ++it){
        if(!it->second.empty()){
            bid = it->first;
            bidsMissing = false;
            break;
        }
    }

    unsigned short ask;
    for (auto& [price, book] : sellLimits){
        if(!book.empty()){
            ask = price;
            asksMissing = false;
            break;
        }
    }
    
    return Spread{bidsMissing, asksMissing, bid, ask};
}

const Depth Matcher::getDepth(){
    const int maxBinsPerSide = 30;
    Depth depth;

    // Bids: iterate highest -> lowest, accumulate cumulative qty
    unsigned int cumQty = 0;
    int bins = 0;
    for (auto it = buyLimits.rbegin(); it != buyLimits.rend() && bins < maxBinsPerSide; ++it){
        unsigned short price = it->first;
        unsigned int totalQtyAtPrice = 0;
        for (auto& o : it->second){
            totalQtyAtPrice += o.unfilled();
        }
        if(totalQtyAtPrice == 0) continue;
        cumQty += totalQtyAtPrice;
        depth.bidBins.push_back(PriceBin{price, cumQty});
        ++bins;
    }

    // Asks: iterate lowest -> highest, accumulate cumulative qty
    cumQty = 0;
    bins = 0;
    for (auto& [price, book] : sellLimits){
        if(bins >= maxBinsPerSide) break;
        unsigned int totalQtyAtPrice = 0;
        for (auto& o : book){
            totalQtyAtPrice += o.unfilled();
        }
        if(totalQtyAtPrice == 0) continue;
        cumQty += totalQtyAtPrice;
        depth.askBins.push_back(PriceBin{price, cumQty});
        ++bins;
    }

    return depth;
}
const std::unordered_map<OrdType, int> Matcher::getOrderCounts(){
    std::unordered_map<OrdType, int> counts{
        {MARKET, 0},
        {LIMIT, 0},
        {STOP, 0},
        {STOPLIMIT, 0}
    };
    std::vector<Order> allOrders{};
    dumpOrdersTo(allOrders);

    for(auto& order : allOrders){
        counts[order.type]++;
    }
    return counts;

}

void Matcher::addOrder(Order& order, bool thenMatch)
{   
    // Exit early and send notifications if order is invalid
    if(!validateOrder(order)){
        return;
    }

    order.ordNum = ++lastOrdNum;

    // TODO mutex that locks the book until orders are added, and matched
    // TODO accumulate incoming orders with 1 thread (up to some limit); place in bulk and match with another thread
    // TODO how does this effect ordering?
    // Mark orders with group number, then compare timestamps within that group to prevent markets from being matched with future limits (only within that group)
    // Does this screw with stops? depends on how spread is updated while group is matched. might be ok

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

    lastOrdNum = order.ordNum;
    this->notifier->notifyOrderPlaced(order);

    if(thenMatch){
        matchOrders();
    }
};

void Matcher::cancelOrder(long ordId){
    canceledOrderIds.insert(ordId);
}

bool Matcher::shouldCleanCanceledOrder(long ordId){
    if(canceledOrderIds.size() == 0){
        return false;
    }

    if(canceledOrderIds.find(ordId) != canceledOrderIds.end()){
        canceledOrderIds.erase(ordId);
        return true;
    }

    return false;
}

void Matcher::dumpOrdersTo(std::vector<Order>& orders){
    
    // Add market and stop orders
    for(auto order : marketOrders){
        orders.push_back(order);
    }

    // Add buy limits and stop limits
    for(auto& [price, book] : buyLimits){
        for(auto order : book){
            orders.push_back(order);
        }
    }

    // Add sell limits and stop limits
    for(auto& [price, book] : sellLimits){
        for(auto order : book){
            orders.push_back(order);
        }
    }
}

void Matcher::pushBackLimitOrder(const Order& order){

    const int reserveLimits = 16;

    switch(order.side)
    {
        case SELL: {
            auto it = sellLimits.find(order.price);
            if (it == sellLimits.end()) {
                auto res = sellLimits.insert(std::make_pair(order.price, std::vector<Order>{}));
                it = res.first;
                it->second.reserve(reserveLimits); // Reserve a few extra elements
            }
            it->second.push_back(order);
            break;
        }
        case BUY: {
            auto it = buyLimits.find(order.price);
            if (it == buyLimits.end()) {
                auto res = buyLimits.insert(std::make_pair(order.price, std::vector<Order>{}));
                it = res.first;
                it->second.reserve(reserveLimits);
            }
            it->second.push_back(order);
            break;
        }
    }
}

bool Matcher::validateOrder(const Order& order){

    // Prevent orders with 0 or negative prices or quantities from being added to the book
    if(order.qty < 1){
        this->notifier->notifyOrderPlacementFailed(order,
            "Can't add order with qty less than 1");
        return false;
    }

    switch (order.type)
    {
        case STOP:
        case STOPLIMIT:
        if (order.stopPrice < 1) {
            this->notifier->notifyOrderPlacementFailed(order,
                "Can't add stop order with stopPrice less than 1");
            return false;
        }
        default:
    }

    switch (order.type)
    {
        case LIMIT:
        case STOPLIMIT:
        if (order.price < 1) {
            this->notifier->notifyOrderPlacementFailed(order,
                "Can't add limit order with price less than 1");
            return false;
        }
        default:
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
    if(marketOrders.empty()){
        return; // Exit early if there are now market orders
    }
    std::vector<size_t> marketOrdersToRemove{};
    Spread spread = getSpread();

    size_t ordIdx = -1;
    for(auto& order : marketOrders){
        ordIdx++;

        // Ignore canceled order, and mark for removal
        if(shouldCleanCanceledOrder(order.ordId)){
            marketOrdersToRemove.push_back(ordIdx);
            continue;
        }

        // Skip attempts to match orders if we can
        if(spread.asksMissing && spread.bidsMissing) break;
        if(spread.asksMissing && order.side == BUY){
            continue;
        }
        if(spread.bidsMissing && order.side == SELL){
            continue;
        }

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
            marketOrdersToRemove.push_back(ordIdx);
        }
    }

    removeIdxs(marketOrders, marketOrdersToRemove);
};

bool Matcher::tryFillBuyMarket(Order& marketOrd, Spread& spread){
    bool marketOrderFilled = false;
    std::vector<unsigned short> limitPricesToRemove{};

    // Iterate through sell limit price buckets, lowest to highest
    for (auto& [price, book] : sellLimits){
        if(book.empty()){
            limitPricesToRemove.push_back(price);
            continue;
        }
        spread.lowestAsk = price;
        marketOrderFilled = matchLimits(marketOrd, spread, book);

        if(marketOrderFilled){
            break;
        }
    }

    removeLimitsByPrice(limitPricesToRemove, SELL);
    return marketOrderFilled;
}

bool Matcher::tryFillSellMarket(Order& marketOrd, Spread& spread){
    bool marketOrderFilled = false;
    std::vector<unsigned short> limitPricesToRemove{};

    // Iterate through buy limit price buckets, highest to lowest
    for (auto it = buyLimits.rbegin(); it != buyLimits.rend(); ++it){
        unsigned short price = it->first;

        if(it->second.empty()){
            limitPricesToRemove.push_back(price);
            continue;
        }

        spread.highestBid = price;

        marketOrderFilled = matchLimits(marketOrd, spread, it->second);
        if(marketOrderFilled){
            break;
        }
    }

    removeLimitsByPrice(limitPricesToRemove, BUY);
    return marketOrderFilled;
}

void Matcher::removeLimitsByPrice(std::vector<unsigned short> limitPricesToRemove, Side side){
    
    if(limitPricesToRemove.empty()){
        return; // early return if there are no limit prices to remove
    }
    
    switch(side){
        case SELL:
            for(auto price : limitPricesToRemove){
                if(sellLimits[price].size()){
                    throw std::logic_error("Can't remove non-empty list of limits!");
                }
                sellLimits.erase(price);
            }
            break;
        case BUY:
            for(auto price : limitPricesToRemove){
                if(buyLimits[price].size()){
                    throw std::logic_error("Can't remove non-empty list of limits!");
                }
                buyLimits.erase(price);
            }
            break;
    }
}

bool Matcher::matchLimits(Order& marketOrd, const Spread& spread, 
    std::vector<Order>& limitOrds){ 
    std::vector<size_t> limitsToRemove;
    bool marketOrdFilled = false;
    size_t limitOrdsSize = limitOrds.size();

    int ordIdx = -1;
    for(auto& limitOrder : limitOrds){
        ordIdx++;

        // Ignore canceled order, and mark for removal
        if(shouldCleanCanceledOrder(limitOrder.ordId)){
            limitsToRemove.push_back(ordIdx);
            continue;
        }

        if(!limitOrder.treatAsLimit(spread)){
            continue;
        }

        auto typeFilled = matchMarketAndLimit(marketOrd, limitOrder);
        
        if (typeFilled.limit){
            limitsToRemove.push_back(ordIdx);
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
    unsigned int limUnFill = limitOrd.unfilled();
    unsigned int markUnFill = marketOrd.unfilled();
    unsigned int fillThisMatch = 0;
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

void removeIdxs(std::vector<Order>& orders, const std::vector<size_t>& idxToRemove){
    if(idxToRemove.empty()) return;

    size_t write = 0;
    size_t prev = 0;
    size_t n = orders.size();

    for(size_t i = 0; i < idxToRemove.size(); ++i){
        int rem = idxToRemove[i];
        if(rem < 0) continue;
        size_t remPos = static_cast<size_t>(rem);
        if(remPos >= n) break;

        // move the block [prev, remPos) to write
        for(size_t k = prev; k < remPos; ++k){
            orders[write++] = std::move(orders[k]);
        }
        prev = remPos + 1; // skip the removed index
    }

    // move the tail [prev, n)
    for(size_t k = prev; k < n; ++k){
        orders[write++] = std::move(orders[k]);
    }

    orders.resize(write);
}