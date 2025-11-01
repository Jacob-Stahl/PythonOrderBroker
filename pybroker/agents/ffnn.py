from pybroker.agents.agent import *
from pybroker.agents.agent import Actions, Agent, Observations
from pybroker.models import *
import random
import math
import numpy as np

class FeedForwardNeuralNetwork(Agent):
    """
    A feedforward neural network agent for trading.
    
    This agent uses a simple feedforward neural network to make trading decisions
    based on observations from the market.
    """

    def __init__(self) -> None:
        super().__init__()

        # Each element in the output vector corresponds to a specific action
        self._output_vector_size = 5
        
        #TODO hardcoding this will be problematic if Observations ever changes
        self._input_vector_size = 13 

        # Define the layers of the neural network
        self.layers: list[Layer] = [
            Layer(input_size=self._input_vector_size, output_size=8, activation='relu'),
            Layer(input_size=8, output_size=self._output_vector_size, activation='sigmoid')
        ]

    def policy(self, observations: Observations) -> Actions:
        if observations.level_1_data.best_ask is None or observations.level_1_data.best_bid is None:
            return super().policy(observations)
        
        state = observations.vectorize()
        # For all positive numbers, take logarithm to compress range
        input_vector = [math.log(x + 1) if x >= 0 else -math.log(-x + 1) for x in state]

        # Pass the input vector through the model to get the output
        output = self.forward(input_vector)
        return self.actionize(output, observations)

    def forward(self, input_vector: list[float]) -> list[float]:
        # Simple feedforward pass through the neural network
        a = np.array(input_vector).reshape(-1, 1)  # Column vector
        for layer in self.layers:
            a = layer.forward(a)
        return a.flatten().tolist()

    def actionize(self, output_vector: list[float], observations:Observations) -> Actions:
        
        market_data = observations.level_1_data
        traderId = observations.account.traderId
        
        # Convert the output vector into trading actions
        mean_price = float(market_data.best_ask) + float(market_data.best_bid) / 2

        # Take no action if this activation is below a certain threshold
        place_order:bool = output_vector[0] > 0.5
        if not place_order:
            return Actions(
                orders=[]
            )

        # Choose side and order type
        side:Side = Side.BUY if output_vector[1] > 0.5 else Side.SELL
        type:OrderType = OrderType.LIMIT if output_vector[2] > 0.5 else OrderType.MARKET

        # trade price and order size scale with market data
        if type is OrderType.LIMIT:
            trade_price: int = int(output_vector[3] * 2 * mean_price)
        else:
            trade_price: int = 0
        
        # Place at least 1 order
        amount: int = max(1, int(math.exp(output_vector[4] * 2)))

        # Place a single order
        return Actions(
            orders=[
                Order(
                    traderId=traderId,
                    side=side,
                    type=type,
                    priceCents=trade_price,
                    amount=amount
                )
            ]
        )

    def mutate(self) -> Agent:
        # Mutate the neural network weights and biases
        mutated_agent:FeedForwardNeuralNetwork = deepcopy(self)
        mutated_agent.layers = [layer.mutate() for layer in self.layers]
        return mutated_agent

class Layer:
    def __init__(self, input_size: int, output_size: int, activation: str = 'relu', mutation_strength: float = 0.1) -> None:
        self.input_size = input_size
        self.output_size = output_size
        self.activation = activation
        self.weights = np.random.randn(output_size, input_size) / (input_size * output_size)
        self.biases = np.zeros((output_size, 1))
        self.mutation_strength = mutation_strength
    
    def forward(self, input_vector: np.ndarray) -> np.ndarray:
        z = np.dot(self.weights, input_vector) + self.biases
        return self.activate(z)
    
    def activate(self, z: np.ndarray) -> np.ndarray:
        if self.activation == 'relu':
            return np.maximum(0, z)
        elif self.activation == 'sigmoid':
            return 1 / (1 + np.exp(-z))
        else:
            raise ValueError(f"Unsupported activation function: {self.activation}")
        

    def mutate(self) -> 'Layer':
        """
        Mutate the layer's weights and biases. 
        Use the standard deviation of weights to scale mutation.
        Returns a new Layer instance with mutated weights and biases.
        """

        mutation_strength = np.std(self.weights) * self.mutation_strength
        self.weights += np.random.randn(*self.weights.shape) * mutation_strength
        self.biases += np.random.randn(*self.biases.shape) * mutation_strength
        return self