from pybroker.agents.agent import *
from pybroker.agents.agent import Actions, Observations
from pybroker.models import *
import random
import math
import numpy as np


class RandomMonkey(Agent):
    """
    A monkey on a Bloomberg Terminal
    
    Randomly generates orders based on normal and poisson distributions
    
    Parameters:
    - mean_bid_price: int
        The mean price (in cents) that this agent will place bid orders around
    - bid_price_std: float
        The standard deviation (in cents) that this agent will place bid orders around
    - prefered_ask_price: int
        The mean price (in cents) that this agent will place ask orders around
    - ask_price_std: float
        The standard deviation (in cents) that this agent will place ask orders around
    - order_size_k: int
        The mean order size (in number of shares) that this agent will place orders around
    - bid_fraction: float
        The fraction of orders that will be bids (vs asks)
    - limit_fraction: float
        The fraction of orders that will be limit orders (vs market orders)
    """

    def __init__(self,
                    mean_bid:int = 320,
                    bid_std: float = 4,
                    mean_ask:int = 320,
                    ask_std: float = 4,
                    order_size_k: int = 3,
                    bid_fraction: float = 0.5,
                    limit_fraction: float = 0.5,
                 
                 ) -> None:
        super().__init__()

        self.mean_bid = mean_bid
        self.bid_std = bid_std
        self.mean_ask = mean_ask
        self.ask_std = ask_std
        self.order_size_k = order_size_k
        self.bid_fraction = bid_fraction
        self.limit_fraction = limit_fraction


    def policy(self, observations: Observations) -> Actions:
        traderId = observations.account.traderId
        
         # construct order
        side: Side
        type: OrderType
        priceCents: int = 0
        amount: int = np.random.poisson(self.order_size_k)
        if random.random() < self.bid_fraction: # Buy
            side = Side.BUY
            priceCents = int(np.random.normal(self.mean_bid, self.bid_std))
        else: # Sell
            priceCents = int(np.random.normal(self.mean_ask, self.ask_std))
            side = Side.SELL
        if random.random() < self.limit_fraction: # Limit
            type = OrderType.LIMIT
        else: # Market
            type = OrderType.MARKET
            priceCents = 0 # price is ignored for market orders
        order = Order(
            traderId=traderId,
            side=side,
            type=type,
            priceCents=priceCents,
            amount=amount,
        )

        return Actions(
            orders=[order]
        )