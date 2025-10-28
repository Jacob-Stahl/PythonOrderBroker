"""functions for visualizing the state of the broker"""

from dataclasses import dataclass, field
from enum import Enum
from src.models import Order, OrderType, Match, Side
from src.order_broker import Broker
from typing import Union
from copy import deepcopy

import matplotlib.pyplot as plt
import pandas as pd

#TODO plot L1 order history

def show_account_cash_balances(broker: Broker):
    # fetch all account info
    accounts_info = broker.accounts.values()

    # build DataFrame
    acct_df = pd.DataFrame({
        'traderId': [acct.traderId for acct in accounts_info],
        'cashBalanceCents': [acct.cashBalanceCents for acct in accounts_info]
    })

    # sort descending by cash balance
    acct_df = acct_df.sort_values(by='cashBalanceCents', ascending=False)

    # plot bar chart
    fig, ax = plt.subplots(figsize=(12, 6))
    ax.bar(acct_df['traderId'].astype(str), acct_df['cashBalanceCents'])
    ax.set_xlabel('Trader ID')
    ax.set_ylabel('Cash Balance (cents)')
    ax.set_title('Account Cash Balances After Trading Session')
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.show()

def show_account_asset_balances(broker: Broker, asset:str):

    # fetch all account info
    accounts_info = broker.accounts.values()

    # build DataFrame for asset balances
    asset_df = pd.DataFrame({
        'traderId': [acct.traderId for acct in accounts_info],
        'assetBalance': [acct.portfolio.get(asset, 0) for acct in accounts_info]
    })

    # sort descending by asset balance
    asset_df = asset_df.sort_values(by='assetBalance', ascending=False)

    # plot bar chartss
    fig, ax = plt.subplots(figsize=(12, 6))
    ax.bar(asset_df['traderId'].astype(str), asset_df['assetBalance'])
    ax.set_xlabel('Trader ID')
    ax.set_ylabel(f'{asset} Balance (units)')
    ax.set_title(f'Account {asset} Balances After Trading Session')
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.show()

def depth_chart(broker: Broker, asset: str):

    bid_depth = pd.DataFrame(broker.get_bid_depth(asset))
    ask_depth = pd.DataFrame(broker.get_ask_depth(asset))

    # Plot bid and ask depth curves
    fig, ax = plt.subplots(figsize=(12, 6))
    ax.step(bid_depth['priceCents'], bid_depth['cumAmount'], where='post', label='Bid Depth', color='green')
    ax.step(ask_depth['priceCents'], ask_depth['cumAmount'], where='post', label='Ask Depth', color='red')
    ax.set_xlabel('Price (cents)')
    ax.set_ylabel('Cumulative Amount')
    ax.set_title('Order Book Depth Chart (Level 2)')
    ax.legend()
    plt.show()