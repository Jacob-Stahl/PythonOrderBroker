#pragma once

#include <string>

/// @brief Subset of the order types found here: https://www.onixs.biz/fix-dictionary/4.4/tagNum_40.html
enum OrdType{

    /// @brief matched with the best limit on the book
    MARKET = 1,

    /// @brief buy or sell a specific price
    LIMIT = 2,

    /// @brief matched with the best limit on the book, above/below a desired price threshold
    STOP = 3

};

enum Side {
    BUY = 1,
    SELL = 2,
};

struct Order{

    /// @brief Buy or Sell
    Side side;
    /// @brief Quantity of the order.
    long int qty;
    /// @brief Price of the order in cents.
    long int price;
    /// @brief Symbol
    std::string symbol;
    /// @brief Order type (Market, Limit, Stop).
    OrdType type;
    /// @brief Time the order was recieved by the service
    long int timestamp;

    /// @brief Calculate the total amount of the order.
    /// @return The total amount in cents.
    long int amt();

    /// @brief Determine if the order should be treated as a market order based on the current market price.
    /// @param marketPrice 
    /// @return 
    bool treatAsMarket(const Spread& spread);
};

struct Spread{
    long int highestBid;
    long int lowestAsk;
};