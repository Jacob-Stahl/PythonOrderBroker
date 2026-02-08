#include <emscripten/bind.h>
#include "order.h"
#include "matcher.h"

using namespace emscripten;

EMSCRIPTEN_BINDINGS(eelib_module) {
    enum_<OrdType>("OrdType")
        .value("MARKET", OrdType::MARKET)
        .value("LIMIT", OrdType::LIMIT)
        .value("STOP", OrdType::STOP)
        .value("STOPLIMIT", OrdType::STOPLIMIT);

    enum_<Side>("Side")
        .value("BUY", Side::BUY)
        .value("SELL", Side::SELL);

    value_object<Order>("Order")
        .field("traderId", &Order::traderId)
        .field("ordId", &Order::ordId)
        .field("side", &Order::side)
        .field("qty", &Order::qty)
        .field("price", &Order::price)
        .field("stopPrice", &Order::stopPrice)
        .field("asset", &Order::asset)
        .field("type", &Order::type);

    value_object<Spread>("Spread")
        .field("bidsMissing", &Spread::bidsMissing)
        .field("asksMissing", &Spread::asksMissing)
        .field("highestBid", &Spread::highestBid)
        .field("lowestAsk", &Spread::lowestAsk);

    value_object<PriceBin>("PriceBin")
        .field("price", &PriceBin::price)
        .field("totalQty", &PriceBin::totalQty);

    value_object<Depth>("Depth")
        .field("bidBins", &Depth::bidBins)
        .field("askBins", &Depth::askBins);
        
    // Register std::vector types used in Depth
    register_vector<PriceBin>("VectorPriceBin");
}
