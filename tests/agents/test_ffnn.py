import pytest
from pybroker.agents import FeedForwardNeuralNetwork
from pybroker.models import *
import numpy as np


def test_ffnn_initialization():
    agent = FeedForwardNeuralNetwork()
    assert agent.layers[0].input_size == 13
    assert agent.layers[-1].output_size == 5

def test_ffnn_forward_pass_sigmoid():
    agent = FeedForwardNeuralNetwork()
    input_vector = [0.0] * 13
    output_vector = agent.forward(input_vector)
    assert len(output_vector) == 5
    for value in output_vector:
        assert 0.0 <= value <= 1.0  # Sigmoid activation output range

def test_ffnn_forward_pass_relu():
    agent = FeedForwardNeuralNetwork()
    # Modify the layer to use ReLU for this test
    agent.layers[0].activation = 'relu'
    input_vector = [-1.0, 0.0, 1.0] * 4 + [0.5]  # 13 inputs
    output_vector = agent.forward(input_vector)
    assert len(output_vector) == 5
    for value in output_vector:
        assert value >= 0.0  # ReLU activation output range


def test_mutation_changes_weights_and_biases():
    layer = FeedForwardNeuralNetwork().layers[0]
    original_weights = layer.weights.copy()
    original_biases = layer.biases.copy()
    
    mutated_layer = layer.mutate()
    
    # Check that weights and biases have changed
    assert not np.array_equal(original_weights, mutated_layer.weights)
    assert not np.array_equal(original_biases, mutated_layer.biases)