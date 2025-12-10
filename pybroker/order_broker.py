from dataclasses import dataclass, field, asdict
from enum import Enum
from pybroker.event_publisher import EventPublisher
from pybroker.models import *
from pybroker.order_matching import Matcher
from typing import Union
from copy import deepcopy
import polars as pl
from pybroker.broker_logging import logger, l1_logger

# TODO add single dataclass to encapsulate latest market data. a single method should generate it
# TODO API should be FIX compliant https://www.fixtrading.org/implementation-guide/
# TODO gRPC API?

class Broker:

    def __init__(self, event_publisher: EventPublisher | None = None) -> None:

        self.tick_count: int = 0

        # traderId - account
        self.accounts: dict[int, Account] = {}

        # asset - Matcher
        self.markets: dict[str, Matcher] = {}

        # asset - L1 history DataFrame
        self.l1_hist: dict[str, pl.DataFrame] = {}
        self.l1_hist_buffer: dict[str, list] = {}

        # optionally publish events
        self.event_publisher = event_publisher
        self.tick_bar_aggregator: TickBarAggregator = TickBarAggregator(
            n_ticks=1000, #TODO make configurable. should be able to support multiple bar sizes simultaneously
            event_publisher=event_publisher
        )

    def next_tick(self) -> int:
        self.tick_count += 1
        return self.tick_count

    def open_account(self, traderId: int):
        assert not traderId in self.accounts.keys()
        newAccount = Account(traderId)
        self.accounts[traderId] = newAccount
    
    def close_account(self, traderId: int) -> Account:
        assert traderId in self.accounts.keys()

        # cancel all open orders for this trader
        for asset in self.markets.keys():
            market = self.markets[asset]
            market.cancel_all_orders_for_trader(traderId)

        # pop and return the account
        account = self.accounts[traderId]
        self.accounts.pop(traderId)
        return account
    
    def end_trading_day(self):
        # clear all markets
        for market in self.markets.values():
            market.clear_order_book()

        # reset earmarked funds from all accounts
        for account in self.accounts.values():
            account.earMarkedCashCents = 0
            account.earMarkedAssets = {}

    def create_market(self, asset: str):
        assert not asset in self.markets.keys()
        self.markets[asset] = Matcher()
        self._init_l1_hist(asset)

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
        
        order.tick = self.next_tick()
        logger.info(f"Placing order {asdict(order)}")

        try:
            assert order.tick >= 0
            assert asset in self.markets.keys()
            assert order.traderId in self.accounts.keys()
            assert order.amount >= 0 
            assert order.priceCents >= 0
        except AssertionError as e:
            logger.exception("Invalid Order!")


        account = self.accounts[order.traderId]
        account_snapshot = deepcopy(account)
        isSuccessful: bool = False

        try:
            # validate the order
            self._validate_order(asset, order, self.accounts[order.traderId])
            
            # earmark funds if limit order
            if order.type == OrderType.LIMIT:
                self._earmark_funds_for_limit_order(asset, order)

            # place order
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
            logger.info("Failed to place order. Rolling back changes...")
            # revert account to the previous state if there is an exception
            self.accounts[order.traderId] = account_snapshot
            isSuccessful = False

        if isSuccessful:
            self.broadcast_excecuted_order(asset, order)
        else:
            self.broadcast_failed_order(asset, order)

        return isSuccessful
    
    def broadcast_excecuted_order(self, asset: str, order: Order):
        # update l1 hist
            self._update_l1_hist(asset, order.tick)
            if self.event_publisher:
                self.event_publisher.order_executed(asset, order)
                self.tick_bar_aggregator.add_tick(
                    self.get_level_1_market_data(asset),
                    asset
                )
                
            logger.info(f"Order placed successfully {asdict(order)}")

    def broadcast_failed_order(self, asset: str, order: Order):
        logger.info(f"Order placement failed {asdict(order)}")
        if self.event_publisher:
            self.event_publisher.order_cancelled(asset, order)
        
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


    def _init_l1_hist(self, asset: str):
        if asset not in self.l1_hist.keys():
            self.l1_hist[asset] = pl.DataFrame(
                schema={"best_bid": pl.Int64, "best_ask": pl.Int64, "tick": pl.Int64}
            )
            self.l1_hist_buffer[asset] = []

    def _update_l1_hist(self, asset: str, tick: int):
        self._init_l1_hist(asset)
        new_row = {
            "best_bid": self.get_highest_bid(asset),
            "best_ask": self.get_lowest_ask(asset),
            "tick": tick
        }
        self.l1_hist_buffer[asset].append(new_row)
        # Only convert to DataFrame every N ticks or on demand
        if len(self.l1_hist_buffer[asset]) >= 1000:  # Tune this threshold
            self.l1_hist[asset] = pl.concat([
                self.l1_hist[asset],
                pl.DataFrame(self.l1_hist_buffer[asset])
            ])
            self.l1_hist_buffer[asset] = []
        l1_logger.info(f"{new_row}")

    def get_l1_history(self, asset: str) -> pl.DataFrame:
        assert asset in self.l1_hist.keys(), f"L1 history for asset '{asset}' does not exist"
        # Flush buffer before returning
        if self.l1_hist_buffer[asset]:
            self.l1_hist[asset] = pl.concat([
                self.l1_hist[asset],
                pl.DataFrame(self.l1_hist_buffer[asset])
            ])
            self.l1_hist_buffer[asset] = []
        return self.l1_hist[asset].clone()

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
    
    def get_level_1_market_data(self, asset: str) -> Level1MarketData:
        assert asset in self.markets.keys(), f"Market for asset '{asset}' does not exist"
        return self.markets[asset].get_level_1_market_data()
    

class TickBarAggregator:
    def __init__(self, n_ticks: int,
                 event_publisher: EventPublisher | None = None) -> None:
        self.n_ticks = n_ticks
        self.event_publisher = event_publisher
        self.tick_buffer: list[Level1MarketData] = []

    def add_tick(self, l1_data: Level1MarketData, asset: str) -> None:
        self.tick_buffer.append(l1_data)
        if len(self.tick_buffer) >= self.n_ticks:
            bar = Bar(
                open=self.tick_buffer[0].best_bid if self.tick_buffer[0].best_bid is not None else 0,
                high=max([tick.best_bid for tick in self.tick_buffer if tick.best_bid is not None] or [0]),
                low=min([tick.best_bid for tick in self.tick_buffer if tick.best_bid is not None] or [0]),
                close=self.tick_buffer[-1].best_bid if self.tick_buffer[-1].best_bid is not None else 0,
                volume=0,  # Volume tracking can be implemented as needed
                ticks=len(self.tick_buffer)
            )
            self.tick_buffer = []
            if self.event_publisher:
                self.event_publisher.publish_tick_bar(asset, bar)
