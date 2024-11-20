import os

def get_subdirectories(path):
    subdirs = []
    for dirpath, dirnames, filenames in os.walk(path):
        for dirname in dirnames:
            subdirs.append(os.path.join(dirpath, dirname))
    return subdirs

delete_log_file = True
delete_files_in_all_outputs_directory = True 

dir_current = os.getcwd()
directories = get_subdirectories('.')

files_to_keep = ['aaet.out', 'cflux.out', 'cpool.out']

for directory in directories:

    print(f'Deleting run directories in {directory}')
    os.chdir(directory)
    os.system('rm -rf run* progress*')
    if delete_log_file:
        os.system('rm output*.log')
    if delete_files_in_all_outputs_directory and os.path.exists('all_outputs'):
        print(f'Deleting files in {os.path.join(directory, 'all_outputs')}')

        for file in os.listdir('all_outputs'):
            if file not in files_to_keep:
                os.system('pwd')
                filepath = os.path.join('all_outputs', file)
                os.remove(filepath)
    os.chdir(dir_current)
