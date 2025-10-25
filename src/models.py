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
    traderId: int
    side: Side
    type: OrderType
    priceCents: int
    amount: int
    timestamp: int

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
