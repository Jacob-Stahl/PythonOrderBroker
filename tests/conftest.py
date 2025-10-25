"""
Pytest configuration and fixtures for the order book project.
"""
import pytest
import sys
import os
from pathlib import Path

# Add src directory to path so we can import modules
src_path = Path(__file__).parent.parent / "src"
sys.path.insert(0, str(src_path))

# Import modules for testing - do this after path modification
try:
    from order_matching import Matcher, Order, Side, OrderType, Match
except ImportError as e:
    print(f"Import error: {e}")
    print(f"Python path: {sys.path}")
    print(f"Src path: {src_path}")
    print(f"Src path exists: {src_path.exists()}")
    raise

import polars as pl

@pytest.fixture
def sample_buy_order():
    """Create a sample buy limit order for testing."""
    return Order(
        id=1,
        traderId=100,
        side=Side.BUY,
        type=OrderType.LIMIT,
        priceCents=10000,  # $100.00
        amount=50,
        timestamp=1000000
    )


@pytest.fixture
def sample_sell_order():
    """Create a sample sell limit order for testing."""
    return Order(
        id=2,
        traderId=200,
        side=Side.SELL,
        type=OrderType.LIMIT,
        priceCents=10050,  # $100.50
        amount=30,
        timestamp=1000001
    )


@pytest.fixture
def sample_market_buy_order():
    """Create a sample market buy order for testing."""
    return Order(
        id=3,
        traderId=300,
        side=Side.BUY,
        type=OrderType.MARKET,
        priceCents=10100,  # $101.00
        amount=25,
        timestamp=1000002
    )


@pytest.fixture
def matcher_with_orders(sample_buy_order, sample_sell_order):
    """Create a matcher with some pre-loaded orders."""
    matcher = Matcher()
    matcher.place_limit_order(sample_buy_order)
    matcher.place_limit_order(sample_sell_order)
    return matcher


# Test data fixtures
@pytest.fixture
def test_orders_data():
    """Provide test data for multiple orders."""
    return [
        {
            "id": 1,
            "traderId": 100,
            "side": Side.BUY,
            "type": OrderType.LIMIT,
            "priceCents": 9950,
            "amount": 100,
            "timestamp": 1000000
        },
        {
            "id": 2,
            "traderId": 200,
            "side": Side.SELL,
            "type": OrderType.LIMIT,
            "priceCents": 10050,
            "amount": 75,
            "timestamp": 1000001
        },
        {
            "id": 3,
            "traderId": 300,
            "side": Side.BUY,
            "type": OrderType.LIMIT,
            "priceCents": 10000,
            "amount": 50,
            "timestamp": 1000002
        }
    ]