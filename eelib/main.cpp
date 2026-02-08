#include <iostream>
#include "order.h"
#include "matcher.h"
#include <random>
#include <chrono>
#include <unordered_map>
#include <type_traits>

void benchmarkMatcher();

int main() {
    benchmarkMatcher();
}


template <typename Enum, int maxValue>
Enum random_enum()
{
    static_assert(std::is_enum_v<Enum>);

    static std::mt19937 rng{std::random_device{}()};

    using U = std::underlying_type_t<Enum>;

    constexpr U min = static_cast<U>(1);
    constexpr U max = static_cast<U>(maxValue);

    std::uniform_int_distribution<U> dist(min, max);
    return static_cast<Enum>(dist(rng));
}

// weighted picker: weights.size() == number of enum values (ordered by underlying value starting at 1)
template<typename Enum>
Enum weighted_random_enum(const std::vector<double>& weights) {
    static_assert(std::is_enum_v<Enum>);
    static std::mt19937 rng{std::random_device{}()};
    std::discrete_distribution<size_t> dist(weights.begin(), weights.end());
    size_t idx = dist(rng);           // idx in [0, weights.size()-1]
    using U = std::underlying_type_t<Enum>;
    return static_cast<Enum>(static_cast<U>(idx + 1)); // +1 because enums are assumed to start at 1
}

class OrderFactory{
    long int currentIdTimestamp = 1;
    std::mt19937 gen;
    std::uniform_int_distribution<long> qtyDistrib;
    std::normal_distribution<double> priceDistrib;
    std::normal_distribution<double> stopPriceFactorDistrib;
    double spreadFactor = 10;

    public:
        OrderFactory(){
            qtyDistrib = std::uniform_int_distribution<long>(1, 100);
            priceDistrib = std::normal_distribution<double>(1000, 100);
            stopPriceFactorDistrib = std::normal_distribution<double>(30, 10);
            
            std::random_device device;
            gen = std::mt19937(device());
        }

        Order newOrder(Side side, OrdType type, long qty, long price = 0, long stopPrice = 0){
            Order o{};
            o.traderId = currentIdTimestamp;
            o.ordId = currentIdTimestamp;
            o.side = side;
            o.qty = qty;
            o.price = price;
            o.stopPrice = stopPrice;
            o.asset = "TEST";
            o.type = type;
            o.ordNum = currentIdTimestamp;

            ++currentIdTimestamp;
            return o;
        }

        Order randomOrder(){
            Side side = random_enum<Side, 2>();
            OrdType ordType = weighted_random_enum<OrdType>(
                {
                    1.0, // MARKET
                    1.01, // LIMIT
                    0.0, // STOP
                    0.0, // STOPLIMIT
                }
            );
            long qty = qtyDistrib(gen);
            double price = priceDistrib(gen);
            double stopPriceFactor = stopPriceFactorDistrib(gen);
            double stopPrice;


            switch(side){
                case BUY:
                    price = price - spreadFactor;
                    stopPrice = price + stopPriceFactor;
                    break;
                case SELL:
                    price = price + spreadFactor; 
                    stopPrice = price - stopPriceFactor;
                    break;
            }
            
            return newOrder(side, ordType, qty, price, stopPrice);
        }
};
    

void benchmarkMatcher(){

    InMemoryNotifier notifier;
    Matcher matcher{&notifier};
    OrderFactory ordFactory{};

    int numOrders = 5000000;
    std::vector<Order> orders{};

    for(int i = 0; i <= numOrders; i++){
        orders.push_back(ordFactory.randomOrder());
    }

    std::cout << "Generated orders. Running benchmark..." << std::endl;
    size_t processed = 0;
    auto last_print = std::chrono::steady_clock::now();

    for (auto &order : orders) {
        matcher.addOrder(order);
        ++processed;

        auto now = std::chrono::steady_clock::now();
        if (now - last_print >= std::chrono::seconds(1)) {
            auto counts = matcher.getOrderCounts();
            auto spread = matcher.getSpread();
            std::cout << processed << " orders processed | "
                      << "MARKET:" << counts[MARKET]
                      << " LIMIT:" << counts[LIMIT]
                      << " STOP:" << counts[STOP]
                      << " STOPLIMIT:" << counts[STOPLIMIT]

                      << " | "
                      << "Matches found:" << notifier.matches.size()
                      << " | Spread:";

            if (spread.bidsMissing) {
                std::cout << " bidsMissing";
            } else {
                std::cout << " highestBid:" << spread.highestBid;
            }

            if (spread.asksMissing) {
                std::cout << " asksMissing";
            } else {
                std::cout << " lowestAsk:" << spread.lowestAsk;
            }

            std::cout << "\n";
            last_print = now;
        }
    }
    std::cout << "Done!" << std::endl;
    std::cout << "Matches Found: " << notifier.matches.size() << std::endl;
    std::cout << "Orders Rejected: " << notifier.placementFailedOrders.size() << std::endl;

};