from typing import Union
from dataclasses import dataclass, field, asdict
from enum import Enum
import polars as pl
from random import randint

# buy sell enum
class Side(Enum):
    BUY = 0
    SELL = 1

class OrderType(Enum):
    LIMIT = 0
    MARKET = 1

@dataclass
class Order:
    traderId: int
    side: Side
    type: OrderType
    amount: int
    priceCents: int = 0
    id: int = randint(0, int(2**30))
    tick: int = -1

    def __post_init__(self):
        # Ensure priceCents is 0 for market orders
        assert not (self.type == OrderType.MARKET and self.priceCents != 0), \
                "Market orders must have priceCents set to 0"

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

@dataclass
class Account:
    traderId: int
    cashBalanceCents: int = 0
    
    # asset - amount
    portfolio: dict[str, int] = field(default_factory= dict)

    earMarkedCashCents: int = 0
    earMarkedAssets: dict[str, int] = field(default_factory= dict)

    def __post_init__(self):
        assert self.traderId >= 0
        assert self.cashBalanceCents >= 0
        for asset, amount in self.portfolio.items():
            assert amount >= 0
        assert self.earMarkedCashCents >= 0
        for asset, amount in self.earMarkedAssets.items():
            assert amount >= 0

    def tradable_balance_cents(self) -> int:
        return self.cashBalanceCents - self.earMarkedCashCents
    
    def tradable_asset_amount(self, asset: str) -> int:
        heldAmount = 0
        if asset in self.earMarkedAssets.keys():
            heldAmount = self.earMarkedAssets[asset]
        
        totalAmount = 0
        if asset in self.portfolio.keys():
            totalAmount = self.portfolio[asset]
        
        return totalAmount - heldAmount
    
    def earmarked_asset_amount(self, asset: str) -> int:
        if asset in self.earMarkedAssets.keys():
            return self.earMarkedAssets[asset]
        return 0

    def earmarked_cash_cents(self) -> int:
        return self.earMarkedCashCents
    

@dataclass
class Level1MarketData:
    best_bid: Union[int, None] = None
    best_ask: Union[int, None] = None

    # Statistics over N ticks
    moving_average_5: Union[float, None] = None
    standard_deviation_5: Union[float, None] = None
    moving_average_10: Union[float, None] = None
    standard_deviation_10: Union[float, None] = None
    moving_average_50: Union[float, None] = None
    standard_deviation_50: Union[float, None] = None
    moving_average_100: Union[float, None] = None
    standard_deviation_100: Union[float, None] = None

    