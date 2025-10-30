from pybroker.agents.agent import *
from pybroker.agents.agent import Actions, Observations
from pybroker.models import *
import random
import math
import numpy as np


class RandomTrendFollower(Agent):
    """
    A trend following agent that randomly places orders around the moving average

    Parameters:
    - order_size_k: int
        The mean order size (in number of shares) that this agent will place orders around
    - bid_fraction: float
        The fraction of orders that will be bids (vs asks)
    - limit_fraction: float
        The fraction of orders that will be limit orders (vs market orders)
    """

    def __init__(self,
                    order_size_k: int = 3,
                    bid_fraction: float = 0.5,
                    limit_fraction: float = 0.5,
                 
                 ) -> None:
        super().__init__()

        self.order_size_k = order_size_k
        self.bid_fraction = bid_fraction
        self.limit_fraction = limit_fraction


    def policy(self, observations: Observations) -> Actions:
        traderId = observations.account.traderId


        price_mean = observations.moving_average_100
        price_std = observations.standard_deviation_100
        
         # construct order
        side: Side
        type: OrderType
        priceCents: int = 0
        amount: int = np.random.poisson(self.order_size_k)
        if random.random() < self.bid_fraction: # Buy
            side = Side.BUY
            if price_mean is not None:
                priceCents = int(np.random.normal(price_mean, 2))
            else:
                priceCents = int(np.random.normal(320, 4))
        else: # Sell
            side = Side.SELL
            if price_mean is not None:
                priceCents = int(np.random.normal(price_mean, 2))
            else:
                priceCents = int(np.random.normal(320, 4))
        if random.random() < self.limit_fraction: # Limit
            type = OrderType.LIMIT
        else: # Market
            type = OrderType.MARKET
            priceCents = 0 # price is ignored for market orders
        order = Order(
            traderId=traderId,
            side=side,
            type=type,
            amount=amount,
            priceCents=priceCents
        )

        return Actions(
            orders=[order]
        )