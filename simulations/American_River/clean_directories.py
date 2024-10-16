import os

dir_current = os.getcwd()

delete_log_file = True
delete_all_outputs_directory = False

for directory in [a for a in os.listdir(dir_current) if os.path.isdir(a)]:

    print(f'Deleting run directories in {directory}')
    os.chdir(directory)
    os.system('rm -rf run* progress*')
    if delete_log_file:
        os.system('rm output*.log')
    if delete_all_outputs_directory:
        os.system('rm -rf all_outputs')
    os.chdir(dir_current)
