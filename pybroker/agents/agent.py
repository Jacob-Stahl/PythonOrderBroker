from abc import abstractmethod, ABC
from dataclasses import dataclass, field, asdict
from models import Order, Account
from typing import Union
from copy import deepcopy


## Base Classes ##
@dataclass
class Observations:
    highest_bid_cents: int
    lowest_ask_cents: int

    standard_deviation_100: float
    moving_average_100: float

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

    @abstractmethod
    def copy(self) -> 'Agent':
        # TODO verify this copies the child, not just the parent
        return deepcopy(self)

    @abstractmethod
    def mutate(self) -> 'Agent':
        return self.copy()