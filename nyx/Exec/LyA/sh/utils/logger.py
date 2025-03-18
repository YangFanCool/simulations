import os
import logging
from .file import rm

class Logger:
    def __init__(self, name='app', log_file='app.log'):
        rm(log_file)
        self.logger = logging.getLogger(name)
        self.logger.setLevel(logging.INFO)
        file_handler = logging.FileHandler(log_file)
        file_handler.setLevel(logging.INFO)
        formatter = logging.Formatter('%(message)s')
        file_handler.setFormatter(formatter)
        self.logger.addHandler(file_handler)

    def print(self, level=0, msg=""):
        if level == 0 and msg:
            total_length = 100
            msg_len = len(str(msg))
            separator_length = (total_length - msg_len - 2) // 2
            msg = f"{'=' * separator_length} {msg} {'=' * separator_length if msg_len % 2 == 0 else '=' * (separator_length + 1)}"
            self.logger.info(msg)
        else:
            self.logger.info(f"{' ' * level} {'-' * level} {msg} ")