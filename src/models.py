from typing import Union
from dataclasses import dataclass, field, asdict
from enum import Enum
import polars as pl

# buy sell enum
class Side(Enum):
    BUY = 0
    SELL = 1

class OrderType(Enum):
    LIMIT = 0
    MARKET = 1

@dataclass
class Order:
    id: int
    timestamp: int
    traderId: int
    side: Side
    type: OrderType
    amount: int
    priceCents: int = 0

    def __post_init__(self):
        # Ensure priceCents is 0 for market orders
        assert not (self.type == OrderType.MARKET and self.priceCents != 0), \
                "Market orders must have priceCents set to 0"

class OrderStatus(Enum):
    SETTLED = 0
    PENDING = 1
    ERROR = 2

@dataclass
class SettlementInfo:
    remaining_amount: int
    status: OrderStatus

@dataclass
class Match:
    marketOrder: Order
    limitOrders: list[Order] = field(default_factory= list)

    def fulfils_market_order(self):
        return self.marketOrder.amount == sum([o.amount for o in self.limitOrders])
    
    def limit_orders_total_amount(self) -> int:
        return sum([o.amount for o in self.limitOrders])
    
    def limit_orders_total_value_cents(self) -> int:
        return sum([o.amount * o.priceCents for o in self.limitOrders])
