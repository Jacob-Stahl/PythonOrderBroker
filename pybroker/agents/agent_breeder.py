"""Used to "Breed" new agents using natural selection techniques"""


from pybroker.agents.agent import *
from pybroker.agents.agent import Actions, Observations
from pybroker.models import *
from pybroker.broker_logging import logger
import random
import math
import numpy as np
import networkx as nx

class AgentBreeder():

    """
    The Agent breeder works by managing a pool of agents trading with the broker.
    If an agent meets the kill criteria, its account is closed with the broker and its assets are withdrawn.
    A new agent is created by copying, then mutating one of the living agents.
    """

    def __init__(self,
                 agent_cls: type,
                 traderIds: set[int],
                 broker: Broker,

                 breeder_cash_balance: int = 0,
                 broker_asset_balances: dict[str, int] = {}
                 ) -> None:

        self.agent_cls: type = agent_cls
        self.desired_pool_size = len(traderIds)
        self.agents: dict[int, Agent] = {}
        self.family_tree = nx.DiGraph()
        self._new_traderId = max(traderIds) + 1

        # internal cash/asset pool
        self.cash_balance_cents: int = breeder_cash_balance
        self.asset_balances: dict[str, int] = broker_asset_balances

        # initialize the living agents pool
        for traderId in traderIds:
            self._life(traderId, broker, fresh_agent=True)

    def _generate_new_traderId(self) -> int:
        new_id = self._new_traderId
        self._new_traderId += 1
        return new_id

    def _determine_starting_cash(self) -> int:
        """Determine how much cash each new agent should start with"""
        if self.desired_pool_size == 0:
            return 0
        return self.cash_balance_cents // self.desired_pool_size
    
    def _determine_starting_assets(self, asset:str) -> int:
        """Determine how much of each asset each new agent should start with"""
        if self.desired_pool_size == 0:
            return 0
        if asset not in self.asset_balances.keys():
            return 0
        return self.asset_balances[asset] // self.desired_pool_size

    def _select_parent_id(self) -> int:
        """
        Select a random parent to reproduce
        This is intended to be called after all unfit agents are culled
        """

        return random.choice(list(self.agents.keys()))

    def _life(self, traderId:int, broker:Broker, fresh_agent:bool = False) -> None:
        """
        Create a new agent with a fresh broker account
        """

        assert traderId not in self.agents.keys(), "TraderId already exists in living agents pool"

        parentId:Union[int, None] = None
        if not fresh_agent:
            parentId = self._select_parent_id()
            child = self.agents[parentId].mutate()
            self.agents[traderId] = child
        else:
            spontanious_generation = self.agent_cls()
            self.agents[traderId] = spontanious_generation
        
        logger.debug(f"Created new agent for traderId {traderId} by mutating parent agent")

        # open a new account for the child agent
        broker.open_account(traderId)

        # fund the new account from the breeder's internal pool
        starting_cash = self._determine_starting_cash()
        broker.deposit_cash(traderId, starting_cash)
        self.cash_balance_cents -= starting_cash
        logger.debug(f"Deposited {starting_cash} cents into new agent account {traderId}")

        # fund the new account with assets from the breeder's internal pool
        for asset in self.asset_balances.keys():
            starting_asset_amount = self._determine_starting_assets(asset)
            broker.deposit_asset(traderId, starting_asset_amount, asset)
            self.asset_balances[asset] -= starting_asset_amount
            logger.debug(f"Deposited {starting_asset_amount} of asset {asset} into new agent account {traderId}")

        # update the family tree
        self._add_to_family_tree(parentId, traderId)

    def _death(self, traderId:int, broker:Broker) -> None:
        """Cull the weak"""
        account = broker.close_account(traderId)
        self.cash_balance_cents += account.cashBalanceCents
        
        for asset in account.portfolio.keys():
            if asset not in self.asset_balances.keys():
                self.asset_balances[asset] = 0
            self.asset_balances[asset] += account.portfolio[asset]

    def _add_to_family_tree(self, parentId:Union[int, None], childId:int) -> None:
        if parentId is not None:
            self.family_tree.add_edge(parentId, childId)
            logger.debug(f"Added edge from {parentId} to {childId} in family tree")
        else:
            self.family_tree.add_node(childId)
            logger.debug(f"Added root node {childId} in family tree")

    def _apply_pressure(self, traderId: int, broker:Broker) -> None:
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
    

    