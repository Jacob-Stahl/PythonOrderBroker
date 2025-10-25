from dataclasses import dataclass, field
from enum import Enum
from src.models import Order, OrderType, Match, Side, OrderStatus, SettlementInfo
from src.order_matching import Matcher
from typing import Union
from copy import deepcopy
import polars as pl

@dataclass
class Account:
    traderId: int
    cashBalanceCents: int = 0
    
    # asset - amount
    portfolio: dict[str, int] = field(default_factory= dict)

class Broker:

    # TODO track L1 orderbook history

    def __init__(self) -> None:
        # traderId - account
        self.accounts: dict[int, Account] = {}

        # asset - Matcher
        self.markets: dict[str, Matcher] = {}

        self.l1_hist = pl.DataFrame(
            schema={
                "best_bid" : pl.Int64,
                "best_ask" : pl.Int64,
                "timestamp" : pl.Int64
            }
        )

    def open_account(self, traderId: int):
        assert not traderId in self.accounts.keys() # the other assets should look like this
        newAccount = Account(traderId)
        self.accounts[traderId] = newAccount
    
    def close_account(self, traderId: int) -> Account:
        assert traderId in self.accounts.keys()

        # TODO cancel all pending orders in all markets for this trader

        account = self.accounts[traderId]
        self.accounts.pop(traderId)
        return account

    def create_market(self, asset: str):
        assert not asset in self.markets.keys()
        self.markets[asset] = Matcher()

    def destroy_market(self, asset: str):
        assert asset in self.markets.keys()

        # remove the market for this asset
        self.markets.pop(asset)

        # remove the asset from all account portfolios
        for account in self.accounts.values():
            if account.portfolio.keys().__contains__(asset):
                account.portfolio.pop(asset)

        # TODO return cash held in buy orders to all traders

    def deposit_cash(self, traderId: int, amountCents: int):
        assert amountCents >= 0
        assert traderId in self.accounts.keys()

        self.accounts[traderId].cashBalanceCents += amountCents

    def withdraw_cash(self, traderId: int, amountCents: int):
        assert amountCents >= 0
        assert traderId in self.accounts.keys()
        assert self.accounts[traderId].cashBalanceCents >= amountCents

        self.accounts[traderId].cashBalanceCents -= amountCents

    def deposit_asset(self, traderId: int, amount:int, asset:str):
        assert amount >= 0
        assert traderId in self.accounts.keys()

        acct = self.accounts[traderId]
        if asset not in acct.portfolio:
            acct.portfolio[asset] = 0
        acct.portfolio[asset] += amount

    def withdraw_asset(self, traderId: int, amount:int, asset:str):
        assert amount >= 0
        assert traderId in self.accounts.keys()

        acct = self.accounts[traderId]
        assert asset in acct.portfolio.keys()
        assert acct.portfolio[asset] >= amount

        acct.portfolio[asset] -= amount
        if acct.portfolio[asset] == 0:
            acct.portfolio.pop(asset)


    def get_account_info(self, traderId: int) -> Account:
        # ensure account exists
        assert traderId in self.accounts.keys(), f"Account for trader {traderId} does not exist"
        # return a deep copy to prevent external mutation
        return deepcopy(self.accounts[traderId])

    def place_order(self, 
                    asset: str, 
                    order: Order) -> bool:

        assert asset in self.markets.keys()
        assert order.traderId in self.accounts.keys()
        assert order.amount > 0 
        assert order.priceCents > 0

        originalCashBalanceCents: int = 0
        originalAssetBalance: int = 0

        cashHeldForOrder: int = 0
        assetsHeldForOrder: int = 0

        if order.side == Side.BUY:
            # validate the trader has enough cash to settle the order
            assert self.accounts[order.traderId].cashBalanceCents >= order.amount * order.priceCents
            originalCashBalanceCents = self.accounts[order.traderId].cashBalanceCents
            
            # hold the required cash
            cashHeldForOrder = order.amount * order.priceCents
            self.accounts[order.traderId].cashBalanceCents -= cashHeldForOrder

        elif order.side == Side.SELL:  # Sell
            assert asset in self.accounts[order.traderId].portfolio.keys()
            assert self.accounts[order.traderId].portfolio[asset] >= order.amount
            originalAssetBalance = self.accounts[order.traderId].portfolio[asset]

            # hold the required assets
            assetsHeldForOrder = order.amount
            self.accounts[order.traderId].portfolio[asset] -= assetsHeldForOrder
        else:
            raise Exception(f"Couldn't resolve side")


        # add to order book
        success: bool = False
        try:
            if(order.type == OrderType.LIMIT):
                self.markets[asset].place_limit_order(order)
                success = True   
            elif(order.type == OrderType.MARKET):
                if self.markets[asset].match_market_order(order):
                    match = self.markets[asset].dequeue_match()
                    if match is not None:
                        self._settle_trade(match, asset, cashHeldForOrder, assetsHeldForOrder)
                        success = True
                    else:
                        raise Exception("Failed to dequeue match")
                else:
                    raise Exception("Failed to match market order")
            else:
                raise NotImplementedError(f"Unsupported order type {order.type}")
        except Exception as ex:
            # revert account to the previous state if there is an exception
            if order.side == Side.SELL:
                self.accounts[order.traderId].portfolio[asset] = originalAssetBalance
            else: # BUY
                self.accounts[order.traderId].cashBalanceCents = originalCashBalanceCents
            success = False

        # update l1 hist
        self._update_l1_hist(asset, order.timestamp)
        return success
        
    def get_lowest_ask(self, asset:str) -> Union[None, int]:
        """try to get the lowest ask for a given asset"""
        # ensure market exists for asset
        assert asset in self.markets.keys(), f"Market for asset '{asset}' does not exist"
        try:
            return self.markets[asset].get_lowest_ask()
        except AssertionError:
            # no ask orders available
            return None
        
    def get_highest_bid(self, asset:str) -> Union[None, int]:
        """try to get the highest bid"""
        # ensure market exists for asset
        assert asset in self.markets.keys(), f"Market for asset '{asset}' does not exist"
        try:
            return self.markets[asset].get_highest_bid()
        except AssertionError:
            # no bid orders available
            return None
        
    def get_bid_depth(self, asset:str ) -> dict[str, list]:
        """get the bid depth"""
        assert asset in self.markets.keys(), f"Market for asset '{asset}' does not exist"
        return self.markets[asset].get_bid_depth()

    def get_ask_depth(self, asset:str ) -> dict[str, list]:
        """get the bid depth"""
        assert asset in self.markets.keys(), f"Market for asset '{asset}' does not exist"
        return self.markets[asset].get_ask_depth()


    def _update_l1_hist(self, asset: str, timestamp: int):
        new_row: pl.DataFrame = pl.DataFrame(
            {
                "best_bid" : self.get_highest_bid(asset),
                "best_ask" : self.get_lowest_ask(asset),
                "timestamp" : timestamp
            }
        )

        self.l1_hist = pl.concat([self.l1_hist, new_row])

    def _settle_trade(self, match: Match, asset: str, cashHeldForOrder: int, assetsHeldForOrder: int) -> None:      
        market = match.marketOrder
        account = self.accounts[market.traderId]

        # settle the market‐order side
        if market.side == Side.BUY:
            if not asset in account.portfolio:
                account.portfolio[asset] = 0
            account.portfolio[asset] += market.amount
            updatedBalance = cashHeldForOrder - (market.amount * market.priceCents)
            if updatedBalance < 0:
                raise Exception("Insufficient held cash to settle buy order")
            account.cashBalanceCents += updatedBalance
        else:
            account.cashBalanceCents += market.amount * market.priceCents

        # settle each limit‐order on the other side
        for limit in match.limitOrders:
            other = self.accounts[limit.traderId]
            if market.side == Side.BUY:
                # sellers get cash
                other.cashBalanceCents += limit.priceCents * limit.amount
            else:
                # buyers get asset
                if not asset in other.portfolio:
                    other.portfolio[asset] = 0
                other.portfolio[asset] += limit.amount


    def total_assets_held_in_ask_limits(self, asset: str) -> int:
        assert asset in self.markets.keys(), f"Market for asset '{asset}' does not exist"
        return self.markets[asset].total_assets_held_in_ask_limits()
    
    def total_cash_held_in_bid_limits(self, asset: str) -> int:
        assert asset in self.markets.keys(), f"Market for asset '{asset}' does not exist"
        return self.markets[asset].total_cash_held_in_bid_limits()