from abc import abstractmethod, ABC
from dataclasses import dataclass, field, asdict
from pybroker.models import Order, Account, Level1MarketData
from typing import Union
from copy import deepcopy
from pybroker.order_broker import Broker
import numpy as np

## Base Classes ##
@dataclass
class Observations:
    level_1_data: Level1MarketData

    # Should be a read-only copy of the agent's account info
    account: Account

    def vectorize(self) -> list[float]:
        """Convert all data into fixed length list of floats"""

        data: list = []

        # Level 1 Data
        data.append(self.level_1_data.best_bid if self.level_1_data.best_bid is not None else -1)
        data.append(self.level_1_data.best_ask if self.level_1_data.best_ask is not None else -1)
        data.append(self.level_1_data.moving_average_5 if self.level_1_data.moving_average_5 is not None else -1)
        data.append(self.level_1_data.standard_deviation_5 if self.level_1_data.standard_deviation_5 is not None else -1)
        data.append(self.level_1_data.moving_average_10 if self.level_1_data.moving_average_10 is not None else -1)
        data.append(self.level_1_data.standard_deviation_10 if self.level_1_data.standard_deviation_10 is not None else -1)
        data.append(self.level_1_data.moving_average_50 if self.level_1_data.moving_average_50 is not None else -1)
        data.append(self.level_1_data.standard_deviation_50 if self.level_1_data.standard_deviation_50 is not None else -1)
        data.append(self.level_1_data.moving_average_100 if self.level_1_data.moving_average_100 is not None else -1)
        data.append(self.level_1_data.standard_deviation_100 if self.level_1_data.standard_deviation_100 is not None else -1)

        # Account Data
        data.append(self.account.cashBalanceCents)
        data.append(self.account.earMarkedCashCents)
        data.append(self.account.tradable_balance_cents())

        # TODO: Assets are tricky because there is no fixed set of assets
        # For now we will just ignore them

        return [float(x) for x in data]

@dataclass
class Actions:
    orders: list[Order] = field(default_factory=list)

class Agent(ABC):
    @abstractmethod
    def policy(self, observations: Observations) -> Actions:
        """Return no action by default"""
        return Actions() 

    def copy(self) -> 'Agent':
        # TODO verify this copies the child, not just the parent
        return deepcopy(self)

    def mutate(self) -> 'Agent':
        return self.copy()
    

def observe(broker: Broker, traderId: int, asset: str) -> Observations:
    account = deepcopy(broker.accounts[traderId])

    return Observations(
        level_1_data=broker.get_level_1_market_data(asset),
        account=account
    )