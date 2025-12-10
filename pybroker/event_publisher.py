"""Publish events to MQTT"""

from dataclasses import dataclass, asdict
from paho.mqtt.client import Client as MqttClient
from typing import Any, Callable, Dict, List
from pybroker.models import *
import json


def serilize_dataclass(obj: Any) -> str:
    """Serialize a dataclass to JSON string."""
    if hasattr(obj, "__dataclass_fields__"):
        dict_obj = asdict(obj)

        for key, value in dict_obj.items():
            if isinstance(value, Enum):
                dict_obj[key] = value.name
            else:
                dict_obj[key] = value

        return json.dumps(dict_obj)

    raise TypeError("Object is not a dataclass")

class EventPublisher:
    """Publish events to subscribers."""

    def __init__(self,
                 topic: str = "orderbook",
                 host: str = "localhost",
                 port: int = 1883) -> None:
        
        self.topic = topic
        self.client = MqttClient()
        self.client.connect(host, port, keepalive=60)

    def order_executed(self, asset: str, order: Order) -> None:
        """Order executed event."""

        subtopic = f"{self.topic}/{asset}/order_executed"
        payload = serilize_dataclass(order)
        self.client.publish(subtopic, payload)

    def order_cancelled(self, asset: str, order: Order) -> None:
        """Order cancelled event."""

        subtopic = f"{self.topic}/{asset}/order_cancelled"
        payload = serilize_dataclass(order)
        self.client.publish(subtopic, payload)

    def publish_tick_bar(self, asset: str, tick_bar: Bar) -> None:
        """Publish a tick bar event."""

        subtopic = f"{self.topic}/{asset}/bars/tick"
        payload = serilize_dataclass(tick_bar)
        self.client.publish(subtopic, payload)