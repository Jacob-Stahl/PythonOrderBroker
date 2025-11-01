from abc import abstractmethod, ABC
from dataclasses import dataclass, field, asdict
from pybroker.models import Order, Account, Level1MarketData
from typing import Union
from copy import deepcopy
from pybroker.order_broker import Broker

## Base Classes ##
@dataclass
class Observations:
    level_1_data: Level1MarketData

    # Should be a read-only copy of the agent's account info
    account: Account

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