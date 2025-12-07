import logging

# Application Logging
logger = logging.getLogger("broker")
logger.setLevel(logging.DEBUG)

file_handler = logging.FileHandler("broker.log")
file_handler.setLevel(logging.DEBUG)

# TODO Log format should have a standard event format that can be easily parsed
# follow this example found in Serilog: https://github.com/serilog/serilog-formatting-compact?tab=readme-ov-file
formatter = logging.Formatter("%(asctime)s - %(name)s - %(levelname)s - %(message)s")
file_handler.setFormatter(formatter)

logger.addHandler(file_handler)

# Seperate log for Level 1 Price Data
l1_logger = logging.getLogger("l1_data")
l1_logger.setLevel(logging.INFO)

file_handler = logging.FileHandler("l1_data.log")
file_handler.setLevel(logging.INFO)
file_handler.setFormatter(formatter)
l1_logger.addHandler(file_handler)
