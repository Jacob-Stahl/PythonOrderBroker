"""
Basic import verification tests - these should all pass if the imports work correctly.
"""
import pytest
from src.order_matching import Matcher, pl_row_to_order
from src.models import Order, Side, OrderType, Match
import polars as pl


class TestBasicImports:
    """Verify that all required classes and functions can be imported from the order_matching module."""
    
    def test_matcher_can_be_imported_and_instantiated(self):
        """Verify that the Matcher class can be imported and instantiated."""
        matcher = Matcher()
        assert matcher is not None
        assert hasattr(matcher, 'place_limit_order')
        assert hasattr(matcher, 'match_market_order')
        print("✓ Matcher class imported and instantiated successfully")
    
    def test_order_class_can_be_imported_and_created(self):
        """Verify that the Order class can be imported and instantiated."""
        order = Order(
            id=1,
            traderId=100,
            side=Side.BUY,
            type=OrderType.LIMIT,
            priceCents=10000,
            amount=50,
            timestamp=1000000
        )
        assert order is not None
        assert order.id == 1
        assert order.side == Side.BUY
        assert order.type == OrderType.LIMIT
        print("✓ Order class imported and created successfully")
    
    def test_enums_can_be_imported_and_used(self):
        """Verify that enum classes (Side, OrderType) can be imported and have correct values."""
        # Test Side enum
        assert Side.BUY.value == 0
        assert Side.SELL.value == 1
        
        # Test OrderType enum
        assert OrderType.LIMIT.value == 0
        assert OrderType.MARKET.value == 1
        
        print("✓ Enums (Side, OrderType) imported and working correctly")
    
    def test_match_class_can_be_imported_and_created(self):
        """Verify that the Match class can be imported and instantiated."""
        order = Order(1, 100, Side.BUY, OrderType.MARKET, 10000, 50, 1000000)
        match = Match(marketOrder=order)
        assert match is not None
        assert match.marketOrder == order
        assert match.limitOrders == []
        assert hasattr(match, 'fulfils_market_order')
        print("✓ Match class imported and created successfully")
    
    def test_pl_row_to_order_function_can_be_imported(self):
        """Verify that the pl_row_to_order function can be imported."""
        assert callable(pl_row_to_order)
        print("✓ pl_row_to_order function imported successfully")
    
    def test_all_required_dependencies_available(self):
        """Verify that all required dependencies (polars, dataclasses, enum) are available."""
        import polars as pl
        from dataclasses import dataclass
        from enum import Enum
        
        assert pl is not None
        assert dataclass is not None
        assert Enum is not None
        print("✓ All required dependencies are available")


class TestBasicFunctionality:
    """Basic functionality tests that should work if imports are correct."""
    
    def test_matcher_has_required_attributes_after_initialization(self):
        """Test that matcher has the required attributes after proper initialization."""
        matcher = Matcher()
        
        # Check initial state
        assert len(matcher._bids) == 0
        assert len(matcher._asks) == 0
        assert len(matcher._matches) == 0
        print("✓ Matcher initialized with correct attributes and empty state")
    
    def test_can_create_orders_with_enums(self):
        """Test that we can create orders using the imported enums."""
        buy_order = Order(
            id=1,
            traderId=100,
            side=Side.BUY,  # Using imported enum
            type=OrderType.LIMIT,  # Using imported enum
            priceCents=10000,
            amount=50,
            timestamp=1000000
        )
        
        sell_order = Order(
            id=2,
            traderId=200,
            side=Side.SELL,  # Using imported enum
            type=OrderType.MARKET,  # Using imported enum
            priceCents=10050,
            amount=30,
            timestamp=1000001
        )
        
        assert buy_order.side == Side.BUY
        assert buy_order.type == OrderType.LIMIT
        assert sell_order.side == Side.SELL
        assert sell_order.type == OrderType.MARKET
        print("✓ Orders created successfully using imported enums")


if __name__ == "__main__":
    # Allow running this file directly for quick verification
    print("Running basic import verification tests...")
    pytest.main([__file__, "-v"])