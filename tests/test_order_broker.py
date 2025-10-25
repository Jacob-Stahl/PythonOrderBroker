import pytest
from src.order_broker import Broker
from src.models import Order, Side, OrderType

from src.broker_vis import depth_chart


def test_account_deposit_withdraw():
    broker = Broker()
    broker.open_account(1)
    broker.deposit_cash(1, 1000)
    assert broker.accounts[1].cashBalanceCents == 1000
    broker.withdraw_cash(1, 500)
    assert broker.accounts[1].cashBalanceCents == 500


def test_create_and_destroy_market():
    broker = Broker()
    broker.create_market("AAPL")
    assert "AAPL" in broker.markets
    broker.destroy_market("AAPL")
    assert "AAPL" not in broker.markets


def test_market_bids_with_no_asks():
    """
    3 orders with equal sizes are placed
    The first is an ask-limit.
    The next 2 are bid-market.
    The first bid should go through, and the second should fail.
    """

    broker = Broker()
    asset = "ABC"
    
    # setup accounts
    for tid in (1, 2, 3):
        broker.open_account(tid)
    broker.create_market(asset)
    
    # seller has asset
    amount = 5
    price = 100
    broker.accounts[1].portfolio[asset] = amount

    # place ask limit
    ask_limit = Order(id=1, traderId=1, side=Side.SELL, type=OrderType.LIMIT, priceCents=price, amount=amount, timestamp=1)
    assert broker.place_order(asset, ask_limit)
    
    # buyers deposit cash
    total = price * amount
    broker.deposit_cash(2, total)
    broker.deposit_cash(3, total)

    # first market buy
    bid_market1 = Order(id=2, traderId=2, side=Side.BUY, type=OrderType.MARKET, priceCents=price, amount=amount, timestamp=2)
    assert broker.place_order(asset, bid_market1)
    
    # check balances after first match
    assert broker.accounts[2].portfolio.get(asset, 0) == amount
    assert broker.accounts[1].cashBalanceCents == total
   
    # second market buy should fail
    bid_market2 = Order(id=3, traderId=3, side=Side.BUY, type=OrderType.MARKET, priceCents=price, amount=amount, timestamp=3)
    assert not broker.place_order(asset, bid_market2)
    
    # ensure no assets or loss of cash
    assert broker.accounts[3].portfolio.get(asset, 0) == 0
    assert broker.accounts[3].cashBalanceCents == total

    # There should be no asks or bids left in the orderbook. The total cash and assets should be with the traders
    market = broker.markets[asset]
    assert market._asks.height == 0
    assert market._bids.height == 0

    # Check the total assets held in ask limits
    assert broker.total_assets_held_in_ask_limits(asset) == 0
    assert broker.total_cash_held_in_bid_limits(asset) == 0


def test_market_ask_with_no_bids():
    """
    The same as the above test, but bid and ask are switched
    """

    broker = Broker()
    asset = "ABC"
    
    # setup accounts
    for tid in (1, 2, 3):
        broker.open_account(tid)
    broker.create_market(asset)
    
    # buyer has cash
    amount = 5
    price = 100
    total = price * amount
    broker.deposit_cash(1, total)
   
    # place bid limit
    bid_limit = Order(id=1, traderId=1, side=Side.BUY, type=OrderType.LIMIT, priceCents=price, amount=amount, timestamp=1)
    assert broker.place_order(asset, bid_limit)
    
    # sellers have asset
    for tid in (2, 3):
        broker.accounts[tid].portfolio[asset] = amount
   
    # first market sell
    sell_market1 = Order(id=2, traderId=2, side=Side.SELL, type=OrderType.MARKET, priceCents=price, amount=amount, timestamp=2)
    assert broker.place_order(asset, sell_market1)
   
    # check balances after first match
    assert broker.accounts[1].portfolio.get(asset, 0) == amount
    assert broker.accounts[2].cashBalanceCents == total
    
    # second market sell should fail
    sell_market2 = Order(id=3, traderId=3, side=Side.SELL, type=OrderType.MARKET, priceCents=price, amount=amount, timestamp=3)
    assert not broker.place_order(asset, sell_market2)
    
    # ensure no cash or loss of asset
    assert broker.accounts[3].cashBalanceCents == 0
    assert broker.accounts[3].portfolio.get(asset, 0) == amount

    # There should be no asks or bids left in the orderbook. The total cash and assets should be with the traders
    market = broker.markets[asset]
    assert market._asks.height == 0
    assert market._bids.height == 0

    # Check the total assets held in ask limits
    assert broker.total_assets_held_in_ask_limits(asset) == 0
    assert broker.total_cash_held_in_bid_limits(asset) == 0


def test_large_limits_are_split_correctly():
    """
    6 orders are placed
    The first is a very large bid-limit
    The next 5 are smaller ask-market orders
    All 5 should go through. The last one should complete the large bid
    The size of the remaining bid is checked after each market order
    """
    broker = Broker()
    asset = "LMN"
    
    # setup buyer account and market
    broker.open_account(1)
    broker.deposit_cash(1, 10000)
    broker.create_market(asset)
    
    # place large bid limit
    large_amount = 10
    price = 100
    bid_limit = Order(id=1, traderId=1, side=Side.BUY, type=OrderType.LIMIT, priceCents=price, amount=large_amount, timestamp=1)
    assert broker.place_order(asset, bid_limit)
    
    # setup sellers
    seller_ids = [2, 3, 4, 5, 6]
    for sid in seller_ids:
        broker.open_account(sid)
        broker.accounts[sid].portfolio[asset] = 2
    
    # process small ask market orders
    for i, sid in enumerate(seller_ids, start=2):
        order_id = i
        sell_market = Order(id=order_id, traderId=sid, side=Side.SELL, type=OrderType.MARKET, priceCents=price, amount=2, timestamp=i)
        assert broker.place_order(asset, sell_market)
        expected_remaining = large_amount - 2 * (i - 1)
        bids_df = broker.markets[asset]._bids
        if expected_remaining > 0:
            assert bids_df[0, "amount"] == expected_remaining
        else:
            # no remaining bid orders
            assert bids_df.height == 0

def test_large_market_orders_are_correctly_matched():
    """
    6 orders are placed
    The first 5 are ask-limits
    The last order is a bid-market that is big enough to be matched with the first 4 orders
    The last limit order should be untouched
    """

    broker = Broker()
    asset = "QRS"
    # open seller accounts and buyer account
    for tid in range(1, 6):
        broker.open_account(tid)
    broker.open_account(6)
    broker.create_market(asset)
    price = 100
    amount = 2
    # place ask limit orders for traders 1-5
    for tid in range(1, 6):
        broker.accounts[tid].portfolio[asset] = amount
        limit_order = Order(
            id=tid,
            traderId=tid,
            side=Side.SELL,
            type=OrderType.LIMIT,
            priceCents=price,
            amount=amount,
            timestamp=tid
        )
        assert broker.place_order(asset, limit_order)
    # deposit cash for buyer and place a market buy to match first four orders
    total_amount = amount * 4
    broker.deposit_cash(6, total_amount * price)
    market_order = Order(
        id=6,
        traderId=6,
        side=Side.BUY,
        type=OrderType.MARKET,
        priceCents=price,
        amount=total_amount,
        timestamp=6
    )
    assert broker.place_order(asset, market_order)
    # buyer should receive matched assets
    assert broker.accounts[6].portfolio.get(asset, 0) == total_amount
    # only the fifth ask limit should remain in the orderbook
    asks_df = broker.markets[asset]._asks
    assert asks_df.height == 1
    assert asks_df[0, "id"] == 5
    assert asks_df[0, "amount"] == amount
