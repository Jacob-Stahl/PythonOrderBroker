#pragma once

/// @brief Subset of the order types found here: https://www.onixs.biz/fix-dictionary/4.4/tagNum_40.html
enum OrdType{
    MARKET = 1,
    LIMIT = 2,
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

    /// @brief Calculate the total amount of the order.
    /// @return The total amount in cents.
    long int amt();

    /// @brief Determine if the order should be treated as a market order based on the current market price.
    /// @param marketPrice 
    /// @return 
    bool treatAsMarket(long int marketPrice);
};