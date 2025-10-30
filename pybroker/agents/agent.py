from abc import abstractmethod, ABC
from dataclasses import dataclass, field, asdict
from pybroker.models import Order, Account
from typing import Union
from copy import deepcopy
from pybroker.order_broker import Broker

## Base Classes ##
@dataclass
class Observations:
    highest_bid_cents: Union[int, None]
    lowest_ask_cents: Union[int, None]

    standard_deviation_100: Union[float, None]
    moving_average_100: Union[float, None]

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
    highest_bid = broker.get_highest_bid(asset)
    lowest_ask = broker.get_lowest_ask(asset)

    # get L1 history for asset
    l1_hist = broker.get_l1_history(asset).to_pandas()

    # Average "best_bid" and "best_ask" over the last 100 entries to get price history
    l1_hist['price'] = (l1_hist['best_bid'] + l1_hist['best_ask']) / 2

    stddev_100 = l1_hist['price'].tail(100).std()
    if( stddev_100 != stddev_100):  # NaN check
        stddev_100: Union[float, None] = None
    ma_100 = l1_hist['price'].tail(100).mean()
    if( ma_100 != ma_100):  # NaN check
        ma_100: Union[float, None] = None

    account = deepcopy(broker.accounts[traderId])

    return Observations(
        highest_bid_cents=highest_bid,
        lowest_ask_cents=lowest_ask,
        standard_deviation_100=stddev_100,
        moving_average_100=ma_100,
        account=account
    )