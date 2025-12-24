#include "order.h"
#include <vector>

class INotifier{
    public:
        virtual void notifyOrderPlaced(const Order& order);
        virtual void notifyOrderPlacementFailed(const Order& order, std::string reason);
        virtual void notifyOrderMatched();
};

/// @brief Notifier used for testing
class MockNotifier: public INotifier{
    public:
        void notifyOrderPlaced(const Order& order);
        void notifyOrderPlacementFailed(const Order& order, std::string reason);
        void notifyOrderMatched();
};