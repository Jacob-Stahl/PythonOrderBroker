"""Used to "Breed" new agents using natural selection techniques"""


from pybroker.agents.agent import *
from pybroker.agents.agent import Actions, Observations
from pybroker.models import *
import random
import math
import numpy as np
from typing import ClassVar

class AgentBreeder():

    """
    The Agent breeder works by managing a pool of agents trading with the broker.
    If an agent meets the kill criteria, its account is closed with the broker and its assets are withdrawn.
    A new agent is created by copying, then mutating one of the living agents.
    """

    def __init__(self,
                 agent_cls: type,
                 traderIds: set[int],


                 broker_cash_balance: int = 0,
                 broker_asset_balances: dict[str, int] = {}
                 ) -> None:

        self.pool_size = len(traderIds)
        self.agents: dict[int, Agent] = {}

        # internal cash/asset pool
        self.cash_balance_cents: int = broker_cash_balance
        self.asset_balances: dict[str, int] = broker_asset_balances

        # initialize all agents
        for i in traderIds:
            agent = agent_cls()
            assert issubclass(Agent, agent)
            self.agents[i] = agent


    def life(self ) -> None:
        raise NotImplementedError()

    def death(self, traderId:int, broker:Broker) -> None:
        account = broker.close_account(traderId)
        self.cash_balance_cents += account.cashBalanceCents
        
        for asset in account.portfolio.keys():
            if asset not in self.asset_balances.keys():
                self.asset_balances[asset] = 0
            self.asset_balances[asset] += account.portfolio[asset]

    def apply_pressure(self, traderId: int, broker:Broker) -> None:
        withdrawn_cash = 100
        
        try:
            broker.withdraw_cash(traderId, withdrawn_cash)
        except:
            return
        
        self.cash_balance_cents = withdrawn_cash 

    @staticmethod
    def _should_kill(account: Account) -> bool:
        """kill if tradable balance is less than 1000"""
        return account.tradable_balance_cents() < 1000
    

    