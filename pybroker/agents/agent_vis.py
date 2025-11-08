"""Tools for visualizing agent behavior and performance."""


from pybroker.agents import AgentBreeder
from pybroker.agents import Agent

import networkx as nx
import matplotlib.pyplot as plt

def show_family_tree(breeder: AgentBreeder):
    """Visualize the agent family graph (works for non-trees, no Graphviz)."""
    G = breeder.family_tree
    pos = nx.spring_layout(G, seed=42)  # Use spring layout for any graph
    plt.figure(figsize=(12, 8))
    nx.draw(
        G,
        pos,
        with_labels=False,      # Hide node labels
        arrows=True,
        node_size=100,          # Small nodes
        node_color='lightblue'
    )
    plt.title("Agent Family Graph")
    plt.show()