import time
import numpy as np
from utils.file import re,rm
from utils.logger import Logger
from exp import run_exp

'''
InSituNet
OmB [0.0215, 0.0235]
OmM  [0.12, 0.155]
h [0.55, 0.85]
'''
h_list = np.linspace(0.55, 0.85, 100).tolist()
h_list = [f"{h:.6f}" for h in h_list]

# var_list = ['density']
# # derive_list = ['pressure', 'magvort', 'x_velocity', 'y_velocity', 'z_velocity']
var = 'pressure'
output_root = f'/root/autodl-tmp/nyx_{var}'
re(output_root)
log_root = f'{output_root}/log'
re(log_root)

logger = Logger('app',f'{log_root}/app.log')
logger.print(0, f'exp begin [counts: {len(h_list)}]')
for index, h in enumerate(h_list):
    exp_name = f'exp_{index + 1:03d}'
    logger.print(0, '')
    logger.print(1, f'Running experiment {exp_name}, h is {h}')
    tik = time.time()
    run_exp(log_root, output_root, exp_name, h)
    logger.print(1, f'Done experiment {exp_name}, time: {(time.time() - tik)/60:.4f} mins')
    logger.print(0, '')
    pass
logger.print(0, 'all exp done')

rm('sh/__pycache__')
rm('sh/utils/__pycache__')