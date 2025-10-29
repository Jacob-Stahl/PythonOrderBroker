import logging

logger = logging.getLogger("broker")
logger.setLevel(logging.DEBUG)

file_handler = logging.FileHandler("broker.log")
file_handler.setLevel(logging.DEBUG)


# TODO Log format should have a standard event format that can be easily parsed
# follow this example found in Serilog: https://github.com/serilog/serilog-formatting-compact?tab=readme-ov-file
formatter = logging.Formatter("%(asctime)s - %(name)s - %(levelname)s - %(message)s")
file_handler.setFormatter(formatter)

logger.addHandler(file_handler)