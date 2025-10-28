"""functions for visualizing the state of the broker"""

from dataclasses import dataclass, field
from enum import Enum
from src.models import Order, OrderType, Match, Side, Account
from src.order_broker import Broker
from typing import Union
from copy import deepcopy
import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import polars as pl

def show_account_cash_balances(broker: Broker):
    """ Show total and earmarked cash balances for all accounts """
    # fetch all account info
    accounts_info: list[Account] = [acct for acct in broker.accounts.values()]

    # build DataFrame
    acct_df = pd.DataFrame({
        'traderId': [acct.traderId for acct in accounts_info],
        'cashBalanceCents': [acct.cashBalanceCents for acct in accounts_info],
        'earmarkedCashCents': [acct.earMarkedCashCents for acct in accounts_info]
    })

    # sort descending by cash balance
    acct_df = acct_df.sort_values(by='cashBalanceCents', ascending=False)

    # plot bar chart
    fig, ax = plt.subplots(figsize=(12, 6))
    ax.bar(acct_df['traderId'].astype(str), 
           acct_df['cashBalanceCents'] - acct_df['earmarkedCashCents'], 
           label='Available Cash', color='green')
    ax.bar(acct_df['traderId'].astype(str), 
           acct_df['earmarkedCashCents'], 
           bottom=acct_df['cashBalanceCents'] - acct_df['earmarkedCashCents'], 
           label='Earmarked Cash', color='orange')
    ax.set_xlabel('Trader ID')
    ax.set_ylabel('Cash Balance (cents)')
    ax.set_title('Account Cash Balances After Trading Session')
    ax.legend()
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.show()

def show_account_asset_balances(broker: Broker, asset:str):
    """
    Show total and earmarked asset balances for all accounts
    """
    # fetch all account info
    accounts_info: list[Account] = [acct for acct in broker.accounts.values()]

    # build DataFrame for asset balances
    asset_df = pd.DataFrame({
        'traderId': [acct.traderId for acct in accounts_info],
        'assetBalance': [acct.portfolio.get(asset, 0) for acct in accounts_info],
        'earmarkedAssets': [acct.earMarkedAssets.get(asset, 0) for acct in accounts_info]
    })

    # sort descending by asset balance
    asset_df = asset_df.sort_values(by='assetBalance', ascending=False)

    # plot stacked bar chart (available assets over earmarked assets)
    fig, ax = plt.subplots(figsize=(12, 6))
    ax.bar(asset_df['traderId'].astype(str), 
           asset_df['assetBalance'] - asset_df['earmarkedAssets'], 
           label='Available Assets', color='blue')
    ax.bar(asset_df['traderId'].astype(str), 
           asset_df['earmarkedAssets'], 
           bottom=asset_df['assetBalance'] - asset_df['earmarkedAssets'], 
           label='Earmarked Assets', color='orange')
    ax.set_xlabel('Trader ID')
    ax.set_ylabel(f'Asset Balance of {asset}')
    ax.set_title(f'Account Asset Balances of {asset} After Trading Session')
    ax.legend()
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


def bid_ask_spread_chart(broker: Broker, asset: str):

    l1_hist: pl.DataFrame = broker.l1_hist[asset]

    bid_ask_df = l1_hist.select([
        pl.col('timestamp'),
        pl.col('best_bid'),
        pl.col('best_ask')
    ]).to_pandas()

    fig, ax = plt.subplots(figsize=(12, 6))
    ax.plot(bid_ask_df['timestamp'], bid_ask_df['best_bid'], label='Best Bid Price', color='green')
    ax.plot(bid_ask_df['timestamp'], bid_ask_df['best_ask'], label='Best Ask Price', color='red')
    ax.set_xlabel('Time')
    ax.set_ylabel('Price (cents)')
    ax.set_title('Best Bid and Ask Prices Over Time')
    ax.legend()
    plt.show()

def average_bid_ask_spread_over_time(broker: Broker, asset: str):

    l1_hist = broker.l1_hist[asset].select([
        pl.col('best_bid'),
        pl.col('best_ask'),
        pl.col('timestamp')
    ]).to_pandas()
    
    avg_series = l1_hist.apply(
        lambda row: (row["best_bid"] + row["best_ask"]) / 2
                    if pd.notna(row["best_bid"]) and pd.notna(row["best_ask"])
                    else None,
        axis=1
    )

    # convert None to np.nan so matplotlib skips those segments
    avg_plot = avg_series.astype(float)

    # compute moving average
    ma_window = 100  # number of points in moving average window
    ma_plot = pd.Series(avg_plot).rolling(window=ma_window, min_periods=1).mean()

    fig, ax = plt.subplots(figsize=(12, 6))
    ax.plot(l1_hist["timestamp"], avg_plot, label="Average Bid/Ask")
    ax.plot(l1_hist["timestamp"], ma_plot,
            label=f"{ma_window}-point MA",
            color="red", linestyle="--")
    ax.set_xlabel("Timestamp")
    ax.set_ylabel("Price (cents)")
    ax.set_title("Average of Best Bid and Ask Over Time")
    ax.legend()
    plt.show()