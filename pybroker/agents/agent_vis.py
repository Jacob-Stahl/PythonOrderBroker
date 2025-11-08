"""Tools for visualizing agent behavior and performance."""


from pybroker.agents import AgentBreeder
from pybroker.agents import Agent

import networkx as nx
import matplotlib.pyplot as plt

import networkx as nx
import matplotlib.pyplot as plt

def prune_family_tree_to_living_agents(G: nx.DiGraph, living_agents: list):
    """
    Prune the graph G to only include nodes that are living agents or ancestors of living agents.
    Modifies G in place.
    """
    living_nodes = set(living_agents)
    keep_nodes = set()
    for node in living_nodes:
        keep_nodes.add(node)
        keep_nodes.update(nx.ancestors(G, node))
    nodes_to_remove = [n for n in G.nodes if n not in keep_nodes]
    G.remove_nodes_from(nodes_to_remove)

def get_node_depths(G, root):
    depths = {root: 0}
    queue = [root]
    while queue:
        node = queue.pop(0)
        for child in G.successors(node):
            depths[child] = depths[node] + 1
            queue.append(child)
    return depths

def show_family_tree(breeder: AgentBreeder):
    """
    Visualize the agent family graph with spring layout, initializing positions by node depth.
    Only shows living agents and their ancestors.
    """

    living_agents: list[int] = list(breeder.agents.keys())

    G = breeder.family_tree.copy()
    prune_family_tree_to_living_agents(G, living_agents)

    # Find root (node with in-degree 0)
    roots = [n for n, d in G.in_degree() if d == 0]
    root = roots[0] if roots else list(G.nodes)[0]
    depths = get_node_depths(G, root)
    
    # Assign initial positions: y by depth, x spread evenly
    nodes_by_depth = {}
    for node, depth in depths.items():
        nodes_by_depth.setdefault(depth, []).append(node)
    pos_init = {}
    for depth, nodes in nodes_by_depth.items():
        for i, node in enumerate(nodes):
            x = i / (len(nodes) + 1)
            y = -depth  # Top-down
            pos_init[node] = (x, y)
    
    # Use spring_layout with initial positions
    pos = nx.spring_layout(G, seed=42, pos=pos_init)
    plt.figure(figsize=(12, 8))
    nx.draw(
        G,
        pos,
        with_labels=False,
        arrows=True,
        node_size=100,
        node_color='lightblue'
    )
    plt.title("Agent Family Tree (Living Agents and Ancestors)")
    plt.show()