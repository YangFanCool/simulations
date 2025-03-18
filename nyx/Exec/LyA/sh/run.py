import yt
import os
import glob
import time
import shutil
import subprocess
yt.set_log_level('WARNING')

config_path = 'sh/exp'
output_dir = 'exp'
max_step = 3
min_step = 1

def clean():
    info("Cleaning Started")
    subprocess.run(['pkill', '-f', 'Nyx'])
    file_to_remove_list = ['Back*', '*log*', output_dir]
    for pattern in file_to_remove_list:
        for file_path in glob.glob(pattern):
            if os.path.isfile(file_path):
                print(f'    remove file {file_path}')
                os.remove(file_path)
            elif os.path.isdir(file_path):
                print(f'    remove dir {file_path}')
                shutil.rmtree(file_path)
    
    info("Cleaning Done")
    pass

def run_exp():
    info("Experiment Started")
    os.makedirs(output_dir)
    shutil.copy(config_path, f'{output_dir}/params')
    special_config = [
        f'max_step = {max_step}',
        f'amr.plot_file = {output_dir}/raw/plt',
    ]
    with open(f'{output_dir}/params', 'a') as params_file:
        params_file.write('\n\n' + '# new params')
        for line in special_config:
            params_file.write('\n' + line)
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
    info(f'Experiment done in {(time.time() - start_time) / 60:.2f} minutes')
    return

def post_process():
    info("Transformation Started")
    start_time = time.time()

    # move files
    files_to_move = glob.glob('Back*') + glob.glob('*log*')
    for file_path in files_to_move:
        shutil.move(file_path, os.path.join(output_dir, os.path.basename(file_path)))
        print(f'    Moved: {file_path} to {output_dir}/')

    raw_dir = f'{output_dir}/raw'
    grid_dir = f'{output_dir}/data'
    if os.path.exists(grid_dir):
        shutil.rmtree(grid_dir)
    os.makedirs(grid_dir, exist_ok=True)

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

    for timestep in timesteps_list:
        print(f'    curr time: {timestep}')
        for name in field_name_list:
            ds = yt.load(os.path.join(raw_dir, f'plt{timestep}'), hint='amrex')
            data = ds.r[field_name]
            data_3d = data.reshape((128, 128, 128))
            data_arr = data_3d.flatten(order='F')
            data_arr_binarny = data_arr.astype('<f')
            data_arr_binarny.tofile(f'{grid_dir}/{field_name}/{timestep}.raw',format='<f')
            pass
        pass

    # remove raw data
    shutil.rmtree(raw_dir)
    info(f'Transformation done in {(time.time() - start_time) / 60:.2f} minutes')
    pass

def info(message=""):
    total_length = 100
    if not message:
        print('=' * total_length)
        return
    message_length = len(message)
    separator_length = (total_length - message_length - 2) // 2
    if message_length % 2 == 0:
        print('=' * separator_length + f' {message} ' + '=' * separator_length)
    else:
        print('=' * separator_length + f' {message} ' + '=' * (separator_length + 1))

if __name__ == "__main__":
    clean()
    run_exp()
    post_process()
    pass
