import pytest
from src.order_broker import Broker
from src.models import Order, Side, OrderType

from src.broker_vis import depth_chart


def test_open_account():
    broker = Broker()
    broker.open_account(1)
    assert 1 in broker.accounts
    assert broker.accounts[1].traderId == 1
    assert broker.accounts[1].cashBalanceCents == 0
    assert broker.accounts[1].portfolio == {}

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


def test_ear_marked_cash_is_correct_after_limit_orders():
    broker = Broker()
    asset = "XYZ"
    broker.open_account(1)
    broker.deposit_cash(1, 100000)
    broker.create_market(asset)

    # ear marked cash should start at 0
    assert broker.accounts[1].earMarkedCashCents == 0

    limit_order1 = Order(id=1, traderId=1, side=Side.BUY, type=OrderType.LIMIT, priceCents=100, amount=50, timestamp=1)
    limit_order2 = Order(id=2, traderId=1, side=Side.BUY, type=OrderType.LIMIT, priceCents=150, amount=30, timestamp=2)

    assert broker.place_order(asset, limit_order1)
    assert broker.accounts[1].earmarked_cash_cents() == 100 * 50
    assert broker.accounts[1].tradable_balance_cents() == 100000 - (100 * 50)

    assert broker.place_order(asset, limit_order2)

    expected_earmarked_cash = (100 * 50) + (150 * 30)
    assert broker.accounts[1].earmarked_cash_cents() == expected_earmarked_cash
    assert broker.accounts[1].tradable_balance_cents() == 100000 - expected_earmarked_cash

def test_ear_marked_assets_is_correct_after_limit_orders():
    broker = Broker()
    asset = "XYZ"
    broker.open_account(1)
    broker.accounts[1].portfolio[asset] = 100
    broker.create_market(asset)

    # ear marked assets should start at 0
    assert broker.accounts[1].earmarked_asset_amount(asset) == 0

    limit_order1 = Order(id=1, traderId=1, side=Side.SELL, type=OrderType.LIMIT, priceCents=100, amount=40, timestamp=1)
    limit_order2 = Order(id=2, traderId=1, side=Side.SELL, type=OrderType.LIMIT, priceCents=150, amount=30, timestamp=2)

    assert broker.place_order(asset, limit_order1)
    assert broker.accounts[1].earmarked_asset_amount(asset) == 40
    assert broker.accounts[1].tradable_asset_amount(asset) == 100 - 40

    assert broker.place_order(asset, limit_order2)

    expected_earmarked_assets = 40 + 30
    assert broker.accounts[1].earmarked_asset_amount(asset) == expected_earmarked_assets
    assert broker.accounts[1].tradable_asset_amount(asset) == 100 - expected_earmarked_assets


def test_the_total_cash_and_assets_held_in_limits_are_the_same_as_earmarked_amounts():
    broker = Broker()
    asset = "XYZ"
    broker.open_account(1)
    broker.deposit_cash(1, 100000)
    broker.accounts[1].portfolio[asset] = 100
    broker.create_market(asset)

    limit_order_buy = Order(id=1, traderId=1, side=Side.BUY, type=OrderType.LIMIT, priceCents=200, amount=20, timestamp=1)
    limit_order_sell = Order(id=2, traderId=1, side=Side.SELL, type=OrderType.LIMIT, priceCents=150, amount=30, timestamp=2)

    assert broker.place_order(asset, limit_order_buy)
    assert broker.place_order(asset, limit_order_sell)

    total_cash_in_bids = broker.total_cash_held_in_bid_limits(asset)
    total_assets_in_asks = broker.total_assets_held_in_ask_limits(asset)

    assert total_cash_in_bids == broker.accounts[1].earmarked_cash_cents()
    assert total_assets_in_asks == broker.accounts[1].earmarked_asset_amount(asset)


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

    # check that the correct amount of assets are ear marked
    assert broker.accounts[1].earMarkedAssets[asset] == amount
    
    # buyers deposit cash
    total = price * amount
    broker.deposit_cash(2, total)
    broker.deposit_cash(3, total)

    # first market buy
    bid_market1 = Order(id=2, traderId=2, side=Side.BUY, type=OrderType.MARKET, amount=amount, timestamp=2)
    assert broker.place_order(asset, bid_market1)
    
    # check balances after first match
    assert broker.accounts[2].portfolio.get(asset, 0) == amount
    assert broker.accounts[1].cashBalanceCents == total

    # check that ear marked assets have been reduced to zero
    assert broker.accounts[1].earMarkedAssets.get(asset, 0) == 0
   
    # second market buy should fail
    bid_market2 = Order(id=3, traderId=3, side=Side.BUY, type=OrderType.MARKET, amount=amount, timestamp=3)
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

    # Check that ear marked assets have been reduced to zero
    assert broker.accounts[1].earMarkedAssets.get(asset, 0) == 0
    assert broker.accounts[1].earMarkedCashCents == 0
    assert broker.accounts[2].earMarkedAssets.get(asset, 0) == 0
    assert broker.accounts[2].earMarkedCashCents == 0


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
    sell_market1 = Order(id=2, traderId=2, side=Side.SELL, type=OrderType.MARKET, amount=amount, timestamp=2)
    assert broker.place_order(asset, sell_market1)
   
    # check balances after first match
    assert broker.accounts[1].portfolio.get(asset, 0) == amount
    assert broker.accounts[2].cashBalanceCents == total
    
    # second market sell should fail
    sell_market2 = Order(id=3, traderId=3, side=Side.SELL, type=OrderType.MARKET, amount=amount, timestamp=3)
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

    # Check that ear marked assets have been reduced to zero
    assert broker.accounts[1].earMarkedAssets.get(asset, 0) == 0
    assert broker.accounts[1].earMarkedCashCents == 0
    assert broker.accounts[2].earMarkedAssets.get(asset, 0) == 0
    assert broker.accounts[2].earMarkedCashCents == 0



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
        sell_market = Order(id=order_id, traderId=sid, side=Side.SELL, type=OrderType.MARKET, amount=2, timestamp=i)
        assert broker.place_order(asset, sell_market)
        expected_remaining = large_amount - 2 * (i - 1)
        bids_df = broker.markets[asset]._bids
        if expected_remaining > 0:
            assert bids_df[0, "amount"] == expected_remaining
        else:
            # no remaining bid orders
            assert bids_df.height == 0

    # check that buyer received all assets
    assert broker.accounts[1].portfolio.get(asset, 0) == large_amount

    # Check that ear marked assets have been reduced to zero
    assert broker.accounts[1].earMarkedAssets.get(asset, 0) == 0
    assert broker.accounts[1].earMarkedCashCents == 0
    assert broker.accounts[2].earMarkedAssets.get(asset, 0) == 0
    assert broker.accounts[2].earMarkedCashCents == 0

    # Check the total assets held in ask limits
    assert broker.total_assets_held_in_ask_limits(asset) == 0
    assert broker.total_cash_held_in_bid_limits(asset) == 0

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

    # check that the first four sellers received their cash
    for tid in range(1, 5):
        assert broker.accounts[tid].cashBalanceCents == amount * price
        assert broker.accounts[tid].portfolio.get(asset, 0) == 0

    # check that the last seller still has their asset and no cash
    assert broker.accounts[5].cashBalanceCents == 0
    assert broker.accounts[5].portfolio.get(asset, 0) == amount

    # Check that the buyer received all assets, and has reduced cash balance
    expected_cash_balance = (total_amount * price) - (total_amount * price)
    assert broker.accounts[6].cashBalanceCents == expected_cash_balance
    assert broker.accounts[6].portfolio.get(asset, 0) == total_amount

def test_limit_orders_fail_if_trader_has_insufficient_tradable_assets_or_cash():
    broker = Broker()
    asset = "TUV"
    broker.open_account(1)
    broker.deposit_cash(1, 500)
    broker.create_market(asset)

    # placing a bid limit that exceeds available cash should fail
    large_bid_limit = Order(id=1, traderId=1, side=Side.BUY, type=OrderType.LIMIT, priceCents=100, amount=10, timestamp=1)
    assert not broker.place_order(asset, large_bid_limit)

    # check that no cash was ear marked
    assert broker.accounts[1].earMarkedCashCents == 0

    # check that no limit orders are in the orderbook
    market = broker.markets[asset]
    assert market._bids.height == 0

    # check that asset and cash balances are unchanged
    assert broker.accounts[1].cashBalanceCents == 500
    assert broker.accounts[1].portfolio.get(asset, 0) == 0

    # placing an ask limit that exceeds available assets should fail
    large_ask_limit = Order(id=2, traderId=1, side=Side.SELL, type=OrderType.LIMIT, priceCents=100, amount=10, timestamp=2)
    assert not broker.place_order(asset, large_ask_limit)

    # check that no assets were ear marked
    assert broker.accounts[1].earMarkedAssets.get(asset, 0) == 0

    # check that no limit orders are in the orderbook
    assert market._asks.height == 0

    # check that asset and cash balances are unchanged
    assert broker.accounts[1].cashBalanceCents == 500
    assert broker.accounts[1].portfolio.get(asset, 0) == 0


def test_market_orders_fail_if_trader_has_insufficient_assets_or_cash():
    broker = Broker()
    asset = "WXY"
    broker.create_market(asset)

    broker.open_account(1)
    broker.open_account(2)
    broker.open_account(3)
    broker.open_account(4)

    # accounts 1 & 2 have insufficient cash and assets respectively
    broker.deposit_cash(1, 500)
    broker.accounts[2].portfolio[asset] = 5

    # accounts 3 & 4 place large limit orders
    broker.deposit_cash(3, 2000)
    broker.accounts[4].portfolio[asset] = 20

    large_bid_limit = Order(id=1, traderId=3, side=Side.BUY, type=OrderType.LIMIT, priceCents=100, amount=10, timestamp=1)
    large_ask_limit = Order(id=2, traderId=4, side=Side.SELL, type=OrderType.LIMIT, priceCents=100, amount=10, timestamp=2)
    assert broker.place_order(asset, large_bid_limit)
    assert broker.place_order(asset, large_ask_limit)

    # account 1 places a market buy that exceeds available cash
    large_market_buy = Order(id=3, traderId=1, side=Side.BUY, type=OrderType.MARKET, amount=10, timestamp=3)
    assert not broker.place_order(asset, large_market_buy)

    # total cash and assets held in limits should be unchanged
    assert broker.total_assets_held_in_ask_limits(asset) == 10
    assert broker.total_cash_held_in_bid_limits(asset) == 1000

    # total cash and assets held by account 1 should be unchanged
    assert broker.accounts[1].cashBalanceCents == 500
    assert broker.accounts[1].portfolio.get(asset, 0) == 0

    # account 2 places a market sell that exceeds available assets
    large_market_sell = Order(id=4, traderId=2, side=Side.SELL, type=OrderType.MARKET, amount=10, timestamp=4)
    assert not broker.place_order(asset, large_market_sell)

    # total cash and assets held in limits should be unchanged
    assert broker.total_assets_held_in_ask_limits(asset) == 10
    assert broker.total_cash_held_in_bid_limits(asset) == 1000

    # total cash and assets held by account 2 should be unchanged
    assert broker.accounts[2].portfolio.get(asset, 0) == 5
    assert broker.accounts[2].cashBalanceCents == 0

def test_l1_history_is_recorded_correctly_for_a_single_asset():
    broker = Broker()
    asset = "DEF"
    broker.open_account(1)
    broker.deposit_cash(1, 10000)
    broker.deposit_asset(1, 10000, asset)
    broker.create_market(asset)

    # place limit orders and check L1 history
    limit_order1 = Order(id=1, traderId=1, side=Side.BUY, type=OrderType.LIMIT, priceCents=100, amount=5, timestamp=1)
    broker.place_order(asset, limit_order1)
    l1_hist = broker.l1_hist[asset]
    assert l1_hist.height == 1
    assert l1_hist[0, "best_bid"] == 100
    assert l1_hist[0, "best_ask"] == None
    assert l1_hist[0, "timestamp"] == 1

    limit_order2 = Order(id=2, traderId=1, side=Side.SELL, type=OrderType.LIMIT, priceCents=150, amount=3, timestamp=2)
    broker.place_order(asset, limit_order2)
    l1_hist = broker.l1_hist[asset]
    assert l1_hist.height == 2
    assert l1_hist[1, "best_bid"] == 100
    assert l1_hist[1, "best_ask"] == 150
    assert l1_hist[1, "timestamp"] == 2

    # place a slightly better bid limit
    limit_order3 = Order(id=3, traderId=1, side=Side.BUY, type=OrderType.LIMIT, priceCents=120, amount=2, timestamp=3)
    broker.place_order(asset, limit_order3)
    l1_hist = broker.l1_hist[asset]
    assert l1_hist.height == 3
    assert l1_hist[2, "best_bid"] == 120
    assert l1_hist[2, "best_ask"] == 150
    assert l1_hist[2, "timestamp"] == 3

    # place a better ask limit
    limit_order4 = Order(id=4, traderId=1, side=Side.SELL, type=OrderType.LIMIT, priceCents=130, amount=4, timestamp=4)
    broker.place_order(asset, limit_order4)
    l1_hist = broker.l1_hist[asset]
    assert l1_hist.height == 4
    assert l1_hist[3, "best_bid"] == 120
    assert l1_hist[3, "best_ask"] == 130
    assert l1_hist[3, "timestamp"] == 4


    # try to place a market buy that can be fully matched
    market_order1 = Order(id=5, traderId=1, side=Side.BUY, type=OrderType.MARKET, amount=4, timestamp=5)
    broker.place_order(asset, market_order1)
    l1_hist = broker.l1_hist[asset]
    assert l1_hist.height == 5
    assert l1_hist[4, "best_bid"] == 120
    assert l1_hist[4, "best_ask"] == 150
    assert l1_hist[4, "timestamp"] == 5

    # try to place a market sell that can be fully matched
    market_order2 = Order(id=6, traderId=1, side=Side.SELL, type=OrderType.MARKET, amount=3, timestamp=6)
    broker.place_order(asset, market_order2)
    l1_hist = broker.l1_hist[asset]
    assert l1_hist.height == 6
    assert l1_hist[5, "best_bid"] == 100
    assert l1_hist[5, "best_ask"] == 150
    assert l1_hist[5, "timestamp"] == 6

    # A failed order should not update L1 history
    market_order3 = Order(id=7, traderId=1, side=Side.BUY, type=OrderType.MARKET, amount=1000, timestamp=7)
    broker.place_order(asset, market_order3)
    l1_hist = broker.l1_hist[asset]
    assert l1_hist.height == 6  # no change
    assert l1_hist[5, "best_bid"] == 100
    assert l1_hist[5, "best_ask"] == 150
    assert l1_hist[5, "timestamp"] == 6


def test_l1_history_is_recorded_correctly_for_multiple_assets():
    broker = Broker()
    assets = ["AAA", "BBB"]
    broker.open_account(1)
    broker.deposit_cash(1, 10000)
    for asset in assets:
        broker.create_market(asset)
        broker.deposit_asset(1, 10000, asset)

    # place limit orders in both markets
    limit_orders_AAA = [
        Order(id=1, traderId=1, side=Side.BUY, type=OrderType.LIMIT, priceCents=100, amount=5, timestamp=1),
        Order(id=2, traderId=1, side=Side.SELL, type=OrderType.LIMIT, priceCents=150, amount=3, timestamp=2),
    ]
    limit_orders_BBB = [
        Order(id=3, traderId=1, side=Side.BUY, type=OrderType.LIMIT, priceCents=200, amount=4, timestamp=1),
        Order(id=4, traderId=1, side=Side.SELL, type=OrderType.LIMIT, priceCents=250, amount=6, timestamp=2),
    ]

    for order in limit_orders_AAA:
        broker.place_order("AAA", order)

    for order in limit_orders_BBB:
        broker.place_order("BBB", order)

    # check L1 history for AAA
    l1_hist_AAA = broker.l1_hist["AAA"]
    assert l1_hist_AAA.height == 2
    assert l1_hist_AAA[1, "best_bid"] == 100
    assert l1_hist_AAA[1, "best_ask"] == 150    

    # check L1 history for BBB
    l1_hist_BBB = broker.l1_hist["BBB"]
    assert l1_hist_BBB.height == 2
    assert l1_hist_BBB[1, "best_bid"] == 200
    assert l1_hist_BBB[1, "best_ask"] == 250

def test_close_account():
    broker = Broker()
    broker.open_account(1)
    broker.deposit_cash(1, 1000)
    broker.accounts[1].portfolio["XYZ"] = 50

    # place some limit orders
    asset = "XYZ"
    broker.create_market(asset)
    limit_order1 = Order(id=1, traderId=1, side=Side.BUY, type=OrderType.LIMIT, priceCents=100, amount=5, timestamp=1)
    limit_order2 = Order(id=2, traderId=1, side=Side.SELL, type=OrderType.LIMIT, priceCents=150, amount=10, timestamp=2)
    broker.place_order(asset, limit_order1)
    broker.place_order(asset, limit_order2)

    # closing account should return cash and assets to zero
    broker.close_account(1)
    assert 1 not in broker.accounts

    # check that no limit orders are in the orderbook
    market = broker.markets[asset]
    assert market._asks.height == 0
    assert market._bids.height == 0