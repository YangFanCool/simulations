import os
import yt
import shutil
yt.set_log_level('WARNING')

res = 256
input_dir = './gadget4/'
output_dir = f'./gadget4_{res}_grid/'

if os.path.exists(output_dir):
    shutil.rmtree(output_dir)
os.makedirs(output_dir)

snap_files = sorted([f for f in os.listdir(input_dir) if f.startswith('snap_') and f.endswith('.hdf5')])

for snap_file in snap_files:
    print(f'current time: {snap_file.replace('snap_','').replace('.hdf5','')}')
    ds = yt.load(os.path.join(input_dir, snap_file))
    grid = ds.arbitrary_grid(left_edge=ds.domain_left_edge, right_edge=ds.domain_right_edge, dims=[res, res, res])
    density_grid = grid['PartType0', 'Density']
    data_arr = density_grid.flatten(order='F')
    snap_number = snap_file.split('_')[1].split('.')[0]
    output_file = os.path.join(output_dir, f'{snap_number}.raw')
    data_arr_binary = data_arr.astype('<f')
    data_arr_binary.tofile(output_file, format='<f')
    print(f"Density data from {snap_file} saved to {output_file}")