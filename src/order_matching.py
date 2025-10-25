# https://medium.com/@kshitij2549/python-grpc-walkthrough-fd192009a1c6

from typing import Union
from dataclasses import dataclass, field, asdict
from enum import Enum
import polars as pl
from src.models import Order, OrderType, Match, Side
from copy import deepcopy
import sys

def pl_row_to_order(row: pl.DataFrame) -> Order:
    assert len(row) == 1

    row_dict = row.to_dict(as_series=False)
    # row_dict is a dict of lists, get the first element for each field
    order_kwargs = {k: v[0] for k, v in row_dict.items() if k != "index"}
    # Convert enums from int if necessary
    order_kwargs['side'] = Side(order_kwargs['side'])
    order_kwargs['type'] = OrderType(order_kwargs['type'])
    return Order(**order_kwargs)

class Matcher():
    """Price-Time (FOFO) order matcher"""

    def __init__(self) -> None:

        # Initialize empty DataFrames with the correct schema
        schema = {
            "id": pl.Int64,
            "timestamp": pl.Int64,
            "traderId": pl.Int64,
            "side": pl.Int64,
            "type": pl.Int64,
            "amount": pl.Int64,
            "priceCents": pl.Int64,
        }
        self._bids: pl.DataFrame = pl.DataFrame(schema=schema).with_row_index()
        self._asks: pl.DataFrame = pl.DataFrame(schema=schema).with_row_index()
        self._matches: list[Match] = []

    def get_lowest_ask(self) -> int:
        if self._asks.height > 0:
            return self._asks[0, "priceCents"]
        else:
            raise AssertionError("No ask limit orders found")
        
    def get_highest_bid(self) -> int:
        if self._bids.height > 0:
            return self._bids[0, "priceCents"]
        else:
            raise AssertionError("No bid limit orders found")
        
    def get_bid_depth(self) -> dict[str, list]:
        """
        returns bid depth as a dictionary of lists
        sorted descending by price
        """
        return self._get_depth(self._bids)

    def get_ask_depth(self) -> dict[str, list]:
        """
        returns ask depth as a dictionary of lists
        sorted ascending by price
        """

        return self._get_depth(self._asks)

    def _get_depth(self, df: pl.DataFrame) -> dict[str, list]:
        amount_total: int = 0
        depth: dict[str, list] = {
            "priceCents" : [],
            "cumAmount" : [],
            "timestamp" : []
        }

        # Use named rows to access by column name
        for row in df.iter_rows(named=True):
            amount_total += int(row["amount"])
            depth["priceCents"].append(int(row["priceCents"]))
            depth["cumAmount"].append(amount_total)
            depth["timestamp"].append(row["timestamp"])

        return depth

        
    def dequeue_match(self) -> Union[None, Match]:
        if any(self._matches):
            match = self._matches[0]
            self._matches.__delitem__(0)
            return match
        else:
            return None
        
    def _sort_bids(self):
        self._bids = self._bids.sort(["priceCents", "timestamp", ], descending=[True, False])
    
    def _sort_asks(self):
        self._asks = self._asks.sort(["priceCents", "timestamp"], descending=[False, False])

    def place_limit_order(self, limitOrder: Order):
        """Place limit orders on the correct side. Sort bids and asks after insertion"""

        assert limitOrder.type == OrderType.LIMIT
        new_row: pl.DataFrame = pl.DataFrame(
            asdict(limitOrder)
        ).with_row_index()
        if limitOrder.side == Side.BUY:
            self._bids = pl.concat([self._bids, new_row])
            self._sort_bids()
        else:
            self._asks = pl.concat([self._asks, new_row])
            self._sort_asks()

    def match_market_order(self, marketOrder: Order, 
                           availableCash: int = sys.maxsize,
                           availableAssets: int = sys.maxsize
                           ) -> bool:
        """ 
        Returns True if fully matched, False failed to find a complete match
        
        Args:
            marketOrder (Order): The market order to match
            availableCash (int): The amount of cash available to spend (for buy orders)
            availableAssets (int): The amount of assets available to sell (for sell orders)
        """
        assert marketOrder.type == OrderType.MARKET
        
        amountRemaining: int = marketOrder.amount
        match: Match = Match(marketOrder, [])
        orderIdsToRemove: list[int] = []

        # snapshot the order books so we can rollback if needed
        orig_bids = self._bids.clone()
        orig_asks = self._asks.clone()
        def rollback():
            self._bids = orig_bids
            self._asks = orig_asks

        # track if we have exceeded available funds, if so we need to rollbacks
        exceededAvailableFunds: bool = False
        exceededAvailableAssets: bool = False
        totalCostCents = 0
        totalCostAssets = 0

        if marketOrder.side == Side.BUY:
            for idx in range(len(self._asks)):
                row = self._asks[idx]
                thisLimitOrder = pl_row_to_order(row)
                if amountRemaining <= 0:
                    break

                # determine how much of the limit order we can match. this accounts for split orders
                match_amount = min(amountRemaining, thisLimitOrder.amount)
                match.limitOrders.append(thisLimitOrder)
                amountRemaining -= match_amount
                totalCostCents += match_amount * thisLimitOrder.priceCents
                if totalCostCents > availableCash:
                    # we have exceeded available cash, rollback this match
                    amountRemaining += match_amount
                    totalCostCents -= match_amount * thisLimitOrder.priceCents
                    exceededAvailableFunds = True
                    break

                # if match amount is less than the limit order, subtract the match amount from the order    
                if(match_amount < thisLimitOrder.amount):
                    self._asks[idx, "amount"] = thisLimitOrder.amount - match_amount
                else: # they are equal, remove the limit order  
                    orderIdsToRemove.append(thisLimitOrder.id)    
            # clear out matched limit orders
            orderIdsToRemove.sort(reverse=True)
            self._asks = self._asks.filter(~pl.col("id").is_in(orderIdsToRemove))
        else: # Sell
            for idx in range(len(self._bids)):
                row = self._bids[idx]
                thisLimitOrder = pl_row_to_order(row)
                if amountRemaining <= 0:
                    break
                match_amount = min(amountRemaining, thisLimitOrder.amount)
                amountRemaining -= match_amount
                totalCostAssets += match_amount
                if totalCostAssets > availableAssets:
                    # we have exceeded available assets, rollback this match
                    amountRemaining += match_amount
                    totalCostAssets -= match_amount
                    exceededAvailableAssets = True
                    break
                # if match amount is less than the limit order, subtract the match amount from the order    
                if(match_amount < thisLimitOrder.amount):
                    self._bids[idx, "amount"] = thisLimitOrder.amount - match_amount            
                    partialOrder = deepcopy(thisLimitOrder)
                    partialOrder.amount = match_amount
                    match.limitOrders.append(partialOrder)

                else: # they are equal, remove the limit order  
                    orderIdsToRemove.append(thisLimitOrder.id)    
                    match.limitOrders.append(thisLimitOrder)

            # clear out matched limit orders
            orderIdsToRemove.sort(reverse=True)
            self._bids = self._bids.filter(~pl.col("id").is_in(orderIdsToRemove))

        # check if we exceeded available funds or assets
        if exceededAvailableFunds or exceededAvailableAssets:
            rollback()
            return False

        # validate that match fulfils the order
        if match.fulfils_market_order():
            self._matches.append(match)
            return True
        else:
            rollback()
            return False
        
    def total_assets_held_in_ask_limits(self) -> int:
        return int(self._asks.select("amount").sum().item())
    
    def total_cash_held_in_bid_limits(self) -> int:
        total_cash = 0
        for row in self._bids.iter_rows(named=True):
            total_cash += int(row["amount"]) * int(row["priceCents"])
        return total_cash
