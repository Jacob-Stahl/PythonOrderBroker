#include "notifier.h"
#include <iostream>
#include <string>
#include <string_view>
#include <format>

void MockNotifier::notifyOrderPlaced(const Order& order){
    std::cout << "Order " << order.ordId << " for trader " << order.traderId << " placed";
}
void MockNotifier::notifyOrderPlacementFailed(const Order& order, std::string reason){
    std::cout << "Order " << order.ordId << " for trader " << order.traderId << " failed. Reason: " << reason;
}
void MockNotifier::notifyOrderMatched(const Match& match){
    std::cout << "Matched!";
}