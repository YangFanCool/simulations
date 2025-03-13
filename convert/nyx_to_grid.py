import os
import yt
import shutil
import numpy as np
yt.set_log_level('WARNING')

res = 128
step = 100

input_dir = f'./nyx_{res}_per_{step}'
output_dir = f'./{input_dir}_grid'

plt_folders = [folder for folder in os.listdir(input_dir) if folder.startswith('plt')]
first_folder = plt_folders[0]
first_data_path = os.path.join(input_dir, first_folder)

ds_first = yt.load(first_data_path, hint='amrex')
field_name_list = [i[1] for i in ds_first.field_list if i[0] == 'boxlib' and i[1]!='StateErr']
timesteps_list = [folder.replace('plt','') for folder in os.listdir(input_dir) if folder.startswith('plt')]

if os.path.exists(output_dir):
    shutil.rmtree(output_dir)
os.makedirs(output_dir, exist_ok=True)
for field_name in field_name_list:
    output_folder = os.path.join(output_dir, field_name)
    os.makedirs(output_folder, exist_ok=True)
    pass

def save_to_binary_dat(timestep, field_name):
    ds = yt.load(os.path.join(input_dir,f'plt{timestep}'), hint='amrex')
    data = ds.r[field_name]
    data_3d = data.reshape((res, res, res))
    data_arr = data_3d.flatten(order='F')
    data_arr_binarny = data_arr.astype('<f')
    
    data_arr_binarny.tofile(f'{output_dir}/{field_name}/{timestep}.raw',format='<f')

for timestep in timesteps_list:
    print('=' * 99)
    print(f'curr time: {timestep}')
    for name in field_name_list:
        print(f'     field: {name}')
        save_to_binary_dat(timestep,name)
        pass
    pass