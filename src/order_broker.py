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
        assert not traderId in self.accounts.keys()
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
            if account.earMarkedAssets.keys().__contains__(asset):
                account.earMarkedAssets.pop(asset)

        # TODO return cash held in buy orders to all traders

    def deposit_cash(self, traderId: int, amountCents: int):
        assert amountCents >= 0
        assert traderId in self.accounts.keys()

        self.accounts[traderId].cashBalanceCents += amountCents

    def withdraw_cash(self, traderId: int, amountCents: int):
        assert amountCents >= 0
        assert traderId in self.accounts.keys()
        assert self.accounts[traderId].tradable_balance_cents() >= amountCents

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
        assert acct.tradable_asset_amount(asset) >= amount

        acct.portfolio[asset] -= amount
        if acct.portfolio[asset] == 0:
            acct.portfolio.pop(asset)


    def get_account_info(self, traderId: int) -> Account:
        # ensure account exists
        assert traderId in self.accounts.keys(), f"Account for trader {traderId} does not exist"
        # return a deep copy to prevent external mutation
        return deepcopy(self.accounts[traderId])
    
    @staticmethod
    def _validate_order(asset: str, order: Order, account: Account) -> bool:
        assert order.amount >= 0 
        assert order.priceCents >= 0
        if order.side == Side.BUY and order.type == OrderType.LIMIT:
            assert account.tradable_balance_cents() >= order.amount * order.priceCents, \
                f"Trader {order.traderId} does not have enough cash to place this buy"
        elif order.side == Side.SELL:
            assert account.tradable_asset_amount(asset) >= order.amount, \
                f"Trader {order.traderId} does not have enough of asset '{asset}' to place this sell"
        return True
    
    def _earmark_funds_for_limit_order(self, asset: str, order: Order):
        assert order.type == OrderType.LIMIT
        account = self.accounts[order.traderId]
        if order.side == Side.BUY:
            assert account.tradable_balance_cents() >= order.amount * order.priceCents, \
                f"Trader {order.traderId} does not have enough cash to place this buy"
            account.earMarkedCashCents += order.amount * order.priceCents
        elif order.side == Side.SELL:
            assert account.tradable_asset_amount(asset) >= order.amount, \
                f"Trader {order.traderId} does not have enough of asset '{asset}' to place this sell"
            if asset not in account.earMarkedAssets.keys():
                account.earMarkedAssets[asset] = 0
            account.earMarkedAssets[asset] += order.amount

    def place_order(self, 
                    asset: str, 
                    order: Order) -> bool:

        assert asset in self.markets.keys()
        assert order.traderId in self.accounts.keys()
        assert order.amount >= 0 
        assert order.priceCents >= 0

        account = self.accounts[order.traderId]
        account_snapshot = deepcopy(account)
        isSuccessful: bool = False

        try:
            # validate the order
            self._validate_order(asset, order, self.accounts[order.traderId])
            
            # earmark funds if limit order
            if order.type == OrderType.LIMIT:
                self._earmark_funds_for_limit_order(asset, order)
                             
            # place the order in the market
            if(order.type == OrderType.LIMIT):
                self.markets[asset].place_limit_order(order)
                isSuccessful = True   
            elif(order.type == OrderType.MARKET):

                tradableCashCents = self.accounts[order.traderId].tradable_balance_cents()
                tradableAssetAmount = self.accounts[order.traderId].tradable_asset_amount(asset)
                if self.markets[asset].match_market_order(order, 
                                                          availableCash=tradableCashCents, 
                                                          availableAssets=tradableAssetAmount):
                    match = self.markets[asset].dequeue_match()
                    if match is not None:
                        self._settle_trade(match, asset)
                        isSuccessful = True
                    else:
                        raise Exception("Failed to dequeue match")
                else:
                    raise Exception("Failed to match market order")
            else:
                raise NotImplementedError(f"Unsupported order type {order.type}")
        except Exception as ex:
            # revert account to the previous state if there is an exception
            self.accounts[order.traderId] = account_snapshot
            isSuccessful = False

        if isSuccessful:
            # update l1 hist
            self._update_l1_hist(asset, order.timestamp)


        return isSuccessful
        
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

    def _settle_trade(self, match: Match, asset: str) -> None:      
        market = match.marketOrder
        account = self.accounts[market.traderId]

        # settle the market‐order side
        if market.side == Side.BUY:
            # debit cash from buyer
            account.cashBalanceCents -= match.limit_orders_total_value_cents()
            
            # credit asset to buyer
            if asset not in account.portfolio.keys():
                account.portfolio[asset] = 0
            account.portfolio[asset] += match.limit_orders_total_amount()

        elif market.side == Side.SELL:
            # debit asset from seller
            account.portfolio[asset] -= match.limit_orders_total_amount()

            # credit cash to seller
            account.cashBalanceCents += match.limit_orders_total_value_cents()


        # settle each limit‐order on the other side - any Exceptions here will create or destroy assets / cash!
        for limit in match.limitOrders:
            limit_account = self.accounts[limit.traderId]

            if limit.side == Side.BUY:
                # debit cash from buyer
                limit_account.earMarkedCashCents -= limit.amount * limit.priceCents
                limit_account.cashBalanceCents -= limit.amount * limit.priceCents

                # credit asset to buyer
                if asset not in limit_account.portfolio.keys():
                    limit_account.portfolio[asset] = 0
                limit_account.portfolio[asset] += limit.amount

            elif limit.side == Side.SELL:
                # debit asset from seller
                limit_account.earMarkedAssets[asset] -= limit.amount
                limit_account.portfolio[asset] -= limit.amount

                # credit cash to seller
                limit_account.cashBalanceCents += limit.amount * limit.priceCents


    def total_assets_held_in_ask_limits(self, asset: str) -> int:
        assert asset in self.markets.keys(), f"Market for asset '{asset}' does not exist"
        return self.markets[asset].total_assets_held_in_ask_limits()
    
    def total_cash_held_in_bid_limits(self, asset: str) -> int:
        assert asset in self.markets.keys(), f"Market for asset '{asset}' does not exist"
        return self.markets[asset].total_cash_held_in_bid_limits()