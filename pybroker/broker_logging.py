import logging
import json
import paho.mqtt.client as mqtt
from dataclasses import dataclass

# Loggers are created at module level
# They can be optionally enabled/configured via setup_logging function
logger = logging.getLogger("broker")
l1_logger = logging.getLogger("l1_data")

@dataclass
class LogSettings:
    mqtt_host: str = "localhost"
    mqtt_port: int = 1883
    mqtt_log_topic: str = "orderbook/logs"
    mqtt_l1_topic: str = "orderbook/l1_data"

    file_log_path: str = "broker.log"
    file_l1_path: str = "l1_data.log"

    enable_mqtt_logging: bool = False
    enable_file_logging: bool = False

class MqttHandler(logging.Handler):
    """Publish log records to an MQTT topic."""
    def __init__(self, topic, host="localhost", port=1883):
        super().__init__()
        self.topic = topic
        self.client = mqtt.Client()

        # Connect to MQTT broker, keep connection alive for up to 60 seconds
        self.client.connect(host, port, keepalive=60)

    def emit(self, record: logging.LogRecord):
        msg = self.format(record)
        self.client.publish(self.topic, msg)

def setup_logging(settings: LogSettings):
    """Setup logging based on provided settings."""

    logger.setLevel(logging.DEBUG)
    l1_logger.setLevel(logging.INFO)
    
    # software logs
    log_formatter = logging.Formatter("%(asctime)s - %(name)s - %(levelname)s - %(message)s")
    if settings.enable_file_logging:
        file_handler = logging.FileHandler(settings.file_log_path)
        file_handler.setLevel(logging.DEBUG)
        file_handler.setFormatter(log_formatter)
        logger.addHandler(file_handler)

    if settings.enable_mqtt_logging:
        mqtt_handler = MqttHandler(settings.mqtt_log_topic, settings.mqtt_host, settings.mqtt_port)
        mqtt_handler.setLevel(logging.INFO)
        mqtt_handler.setFormatter(log_formatter)
        logger.addHandler(mqtt_handler)

    # level 1 order book events
    if settings.enable_file_logging:
        l1_file_handler = logging.FileHandler(settings.file_l1_path)
        l1_file_handler.setLevel(logging.INFO)
        l1_logger.addHandler(l1_file_handler)

    if settings.enable_mqtt_logging:
        l1_mqtt_handler = MqttHandler(settings.mqtt_l1_topic, settings.mqtt_host, settings.mqtt_port)
        l1_mqtt_handler.setLevel(logging.INFO)
        l1_logger.addHandler(l1_mqtt_handler)

