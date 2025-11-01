from pybroker.agents.agent import *
from pybroker.agents.agent import Actions, Observations
from pybroker.models import *
from random import shuffle

class Arbitrage(Agent):
    """
    If this agent observes that the highest bid is greater than the lowest ask,
    It places a small market buy and sell in a random order
    """

    def __init__(self) -> None:
        super().__init__()

    def policy(self, observations: Observations) -> Actions:     
        # "Trust me, this is my account"
        traderId = observations.account.traderId

        if( observations.level_1_data.best_bid is None or observations.level_1_data.best_ask is None):
            return super().policy(observations)

        if observations.level_1_data.best_bid > observations.level_1_data.best_ask:
            orders: list[Order] = [
                Order(
                    traderId=traderId,
                    side=Side.BUY,
                    type=OrderType.MARKET,
                    amount=1,
                ),
                Order(
                    traderId=traderId,
                    side=Side.SELL,
                    type=OrderType.MARKET,
                    amount=1,
                ),
            ]

            shuffle(orders)
            return Actions(
                orders=orders
            )

        else:
            return super().policy(observations)