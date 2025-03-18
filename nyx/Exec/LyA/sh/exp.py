import yt
import os
import glob
import time
import shutil
import subprocess
from utils.file import re,rm
from utils.logger import Logger

yt.set_log_level('WARNING')

def run_exp(log_root, output_root, exp_name, h):  

    output_dir = f'{output_root}/{exp_name}'
    re(f'{output_root}/{exp_name}')

    exp_logger = Logger(exp_name, f'{log_root}/{exp_name}.log')
    exp_logger.print(0, f'experiment {exp_name} started, h is {h}')
    exp_logger.print(0, f'all the output file in {output_dir}')

    config_path = 'sh/base'
    min_step = 150
    max_step = 350
    var_list = ['density']
    derive_list = ['pressure', 'magvort', 'x_velocity', 'y_velocity', 'z_velocity']
    special_config = [
        f'max_step = {max_step}',
        f'amr.data_log = {output_dir}/data_log.log',
        f'amr.plot_file = {output_dir}/raw/plt',
        f'amr.plot_vars = {" ".join(var_list)}',
        f'amr.derive_plot_vars = {" ".join(derive_list)}',
        f'nyx.comoving_h = {h}',
    ]

    # copy the params
    shutil.copy(config_path, f'{output_dir}/params')
    with open(f'{output_dir}/params', 'a') as params_file:
        params_file.write('\n\n' + '# new params')
        for line in special_config:
            params_file.write('\n' + line)
            pass
        pass

    # run the simulation
    start_time = time.time()
    with open(f"{output_dir}/run.log", "w") as log_file:
        process = subprocess.Popen(
            ["nohup", "./Nyx3d.gnu.TPROF.MPI.CUDA.ex", f'{output_dir}/params'],
            stdout=log_file,
            stderr=subprocess.STDOUT,
            preexec_fn=os.setpgrp
        )
        process.wait()
        pass
    exp_logger.print(1, f'simulation done in {(time.time() - start_time) / 60:.2f} minutes')
    shutil.move('./mem_info.log', f'{output_dir}/')
    # post process the data
    raw_dir = f'{output_dir}/raw'
    grid_dir = f'{output_dir}/data'
    re(grid_dir)
    plt_folders = [folder for folder in os.listdir(raw_dir) if folder.startswith('plt')]
    ds_first = yt.load(os.path.join(raw_dir, plt_folders[0]), hint='amrex')
    field_name_list = [i[1] for i in ds_first.field_list if i[0] == 'boxlib' and i[1]!='StateErr']
    timesteps_list = [folder.replace('plt','') for folder in os.listdir(raw_dir) if folder.startswith('plt')]
    timesteps_list = [time for time in timesteps_list if int(time) >= min_step]
    timesteps_list.sort(key=lambda x: int(x))

    for field_name in field_name_list:
        output_folder = os.path.join(grid_dir, field_name)
        os.makedirs(output_folder, exist_ok=True)
        pass

    exp_logger.print(1, f'convert raw plt data ...')
    for timestep in timesteps_list:
        exp_logger.print(2, f'time {timestep}')
        for field_name in field_name_list:
            ds = yt.load(os.path.join(raw_dir, f'plt{timestep}'), hint='amrex')
            data = ds.r[field_name]
            data_3d = data.reshape((128, 128, 128))
            data_arr = data_3d.flatten(order='F')
            data_arr_binarny = data_arr.astype('<f')
            data_arr_binarny.tofile(f'{grid_dir}/{field_name}/{timestep}.raw',format='<f')
            pass
        pass
    exp_logger.print(1, f'convert done ...')
    # cleaning work
    exp_logger.print(1, f'clean all raw plt data ...')
    shutil.rmtree(raw_dir)
    pass
