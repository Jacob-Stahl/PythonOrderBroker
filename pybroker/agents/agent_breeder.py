"""Used to "Breed" new agents using natural selection techniques"""


from pybroker.agents.agent import *
from pybroker.agents.agent import Actions, Observations
from pybroker.models import *
import random
import math
import numpy as np
from typing import ClassVar

class AgentBreeder():
    def __init__(self,
                 agent_cls: type,
                 traderIds: set[int]   
                 ) -> None:

        self.pool_size = len(traderIds)
        self.agents: dict[int, Agent] = {}

        # initialize all agents
        for i in traderIds:
            agent = agent_cls()
            assert issubclass(Agent, agent)
            self.agents[i] = agent


    def cull(self) -> None:
        raise NotImplementedError()

    @staticmethod
    def _should_kill(account: Account) -> bool:
        """kill if tradable balance is less than 1000"""
        return account.tradable_balance_cents() < 1000
    

    