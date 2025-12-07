"""Publish events to MQTT"""

from dataclasses import dataclass, asdict
from paho.mqtt.client import Client as MqttClient
from typing import Any, Callable, Dict, List
from pybroker.models import *

class EventPublisher:
    """Publish events to subscribers."""

    def __init__(self,
                 topic: str,
                 host: str = "localhost",
                 port: int = 1883) -> None:
        
        self.topic = topic
        self.client = MqttClient()
        self.client.connect(host, port, keepalive=60)

    def execute_order(self, order: Order) -> None:
        """Execute an order event."""

        subtopic = f"{self.topic}/order_executed"
        payload = asdict(order)
        self.client.publish(subtopic, str(payload))

    def order_cancelled(self, order: Order) -> None:
        """Order cancelled event."""

        subtopic = f"{self.topic}/order_cancelled"
        payload = asdict(order)
        self.client.publish(subtopic, str(payload))

    def publish_tick_bar(self, asset: str, tick_bar: Bar) -> None:
        """Publish a tick bar event."""

        subtopic = f"{self.topic}/{asset}/bars/tick"
        payload = asdict(tick_bar)
        self.client.publish(subtopic, str(payload))
