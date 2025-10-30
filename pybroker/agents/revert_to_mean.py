from pybroker.agents.agent import *
from pybroker.agents.agent import Actions, Observations
from pybroker.models import *
from random import shuffle
import math

class RevertToMean(Agent):
    """
    This agent places small buy and sell limits below and above the moving average
    """
    
    def __init__(self, sigma: float = 0.5) -> None:
        self.sigma: float = sigma
        super().__init__()

    def policy(self, observations: Observations) -> Actions:
        traderId = observations.account.traderId

        mean = observations.moving_average_100
        std = observations.standard_deviation_100

        if( mean is None or std is None):
            return super().policy(observations)

        buy_price = int(max(0, mean - (std * self.sigma)))
        sell_price = int(max(0, mean + (std * self.sigma)))

        orders: list[Order] = [
            Order(
                traderId=traderId,
                side = Side.BUY,
                type=OrderType.LIMIT,
                amount=1,
                priceCents=buy_price
            ),
            Order(
                traderId=traderId,
                side = Side.SELL,
                type=OrderType.LIMIT,
                amount=1,
                priceCents=sell_price
            ),
        ]

        return Actions(
            orders=orders
        )



