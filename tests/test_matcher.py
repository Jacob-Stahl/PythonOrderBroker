"""
Test the order matcher functionality and verify imports work correctly.
"""
import pytest
from src.order_matching import Matcher, pl_row_to_order
import polars as pl
from src.models import Order, Side, OrderType


class TestMatcher:
    def test_place_buy_limit_order(self, sample_buy_order):
        """Test placing a buy limit order."""
        matcher = Matcher()
        matcher.place_limit_order(sample_buy_order)
        
        assert len(matcher._bids) == 1
        assert len(matcher._asks) == 0
        
        # Verify the order was added correctly
        first_bid = matcher._bids.row(0, named=True)
        assert first_bid['id'] == sample_buy_order.id
        assert first_bid['side'] == Side.BUY.value
        assert first_bid['priceCents'] == sample_buy_order.priceCents
    
    def test_place_sell_limit_order(self, sample_sell_order):
        """Test placing a sell limit order."""
        matcher = Matcher()
        matcher.place_limit_order(sample_sell_order)
        
        assert len(matcher._bids) == 0
        assert len(matcher._asks) == 1
        
        # Verify the order was added correctly
        first_ask = matcher._asks.row(0, named=True)
        assert first_ask['id'] == sample_sell_order.id
        assert first_ask['side'] == Side.SELL.value
        assert first_ask['priceCents'] == sample_sell_order.priceCents

        # check the total assets held in ask limits
        assert matcher.total_assets_held_in_ask_limits() == sample_sell_order.amount
        assert matcher.total_cash_held_in_bid_limits() == 0

    def test_get_ask_depth(self, sample_sell_order):
        """Test get_ask_depth returns correct price levels and cumulative amounts sorted by price asc when timestamps equal."""
        matcher = Matcher()
        matcher.place_limit_order(sample_sell_order)
        order2 = Order(
            id=sample_sell_order.id + 1,
            traderId=sample_sell_order.traderId + 1,
            side=Side.SELL,
            type=OrderType.LIMIT,
            priceCents=sample_sell_order.priceCents - 50,
            amount=15,
            timestamp=sample_sell_order.timestamp
        )
        matcher.place_limit_order(order2)
        depth = matcher.get_ask_depth()
        # With equal timestamps, asks are sorted by price ascending
        assert depth["priceCents"] == [order2.priceCents, sample_sell_order.priceCents]
        assert depth["cumAmount"] == [order2.amount, order2.amount + sample_sell_order.amount]

        # Check the total assets held in ask limits
        assert matcher.total_assets_held_in_ask_limits() == sample_sell_order.amount + order2.amount
        assert matcher.total_cash_held_in_bid_limits() == 0


class TestOrderConversion:
    """Test the pl_row_to_order function."""
    
    def test_pl_row_to_order_conversion(self):
        """Test converting a Polars DataFrame row to an Order object."""
        # Create a single-row DataFrame
        data = {
            'id': [1],
            'traderId': [100],
            'side': [Side.BUY.value],
            'type': [OrderType.LIMIT.value],
            'priceCents': [10000],
            'amount': [50],
            'timestamp': [1000000]
        }
        df = pl.DataFrame(data)
        
        order = pl_row_to_order(df)
        
        assert isinstance(order, Order)
        assert order.id == 1
        assert order.traderId == 100
        assert order.side == Side.BUY
        assert order.type == OrderType.LIMIT
        assert order.priceCents == 10000
        assert order.amount == 50
        assert order.timestamp == 1000000


class TestMatchingLogic:
    """Test the order matching logic."""
    
    def test_market_order_matching_attempt(self, matcher_with_orders, sample_market_buy_order):
        """Test attempting to match a market order (basic functionality test)."""
        matcher = matcher_with_orders
        
        # This should not raise an exception
        result = matcher.match_market_order(sample_market_buy_order)
        
        # Currently returns False as the implementation is incomplete
        # This test verifies the method can be called without errors
        assert isinstance(result, bool)
    
    def test_market_order_assertion(self, sample_buy_order):
        """Test that match_market_order raises assertion for non-market orders."""
        matcher = Matcher()
        
        with pytest.raises(AssertionError):
            matcher.match_market_order(sample_buy_order)  # This is a limit order, should fail