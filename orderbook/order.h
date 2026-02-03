#pragma once

#include <string>

struct Spread{
    bool bidsMissing = true;
    bool asksMissing = true;

    unsigned short highestBid;
    unsigned short lowestAsk;
};

/// @brief Subset of the order types found here: https://www.onixs.biz/fix-dictionary/4.4/tagNum_40.html
enum OrdType{

    /// @brief matched with the best limit on the book
    MARKET = 1,

    /// @brief buy or sell a specific price
    LIMIT = 2,

    /// @brief matched with the best limit on the book, above/below a desired price threshold
    STOP = 3,

    /// @brief matched with the best market on the book, above/below a desired price threshold
    STOPLIMIT = 4
};

enum Side {
    BUY = 1,
    SELL = 2,
};

struct Order{

    /// @brief Id of the trader that placed this order
    long traderId;
    /// @brief Unique id of this order
    long ordId;
    /// @brief Buy or Sell
    Side side;
    /// @brief Quantity of the order.
    unsigned int qty;
    /// @brief Price of the order in cents.
    unsigned short price;
    /// @brief Stop price of the order.
    unsigned short stopPrice;
    /// @brief Symbol
    std::string symbol;
    /// @brief Order type (Market, Limit, Stop).
    OrdType type;

    // TODO add group number for bulk matching

    /// @brief Time the order was recieved by the service
    unsigned long ordNum;
    /// @brief Number of items filled. 
    unsigned int fill = 0;
    /// @brief Calculate the total amount of the order.
    /// @return The total amount in cents.
    const unsigned int amt();

    /// @brief Determine if the order should be treated as a market order based on the current market price.
    /// @param marketPrice 
    /// @return 
    const bool treatAsMarket(const Spread& spread);

    /// @brief Determine if the order should be treated as a limit order based on the current market price
    /// @param spread 
    /// @return 
    const bool treatAsLimit(const Spread& spread);

    const bool fillComplete(){
        return qty == fill;
    }

    const unsigned int unfilled(){
        // A bit dangerous. unfilled should NEVER be negative
        return qty - fill;
    }
};