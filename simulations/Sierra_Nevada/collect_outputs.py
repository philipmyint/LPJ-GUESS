import os

copy_ins_files = True

files_to_copy = ['aaet.out', 'cflux.out', 'cpool.out']
dir_current = os.getcwd()

dir_outputs = 'outputs'
os.system('rm -rf ' + dir_outputs)
os.system('mkdir ' + dir_outputs)

def get_subdirectories(path):
    subdirs = []
    for dirpath, dirnames, filenames in os.walk(path):
        for dirname in dirnames:
            subdirs.append(os.path.join(dirpath, dirname))
    return subdirs

#dirs_to_copy_from = ['']
dirs_to_copy_from = get_subdirectories('.') # Uncomment this line if you want to copy output from all directories

for dir_to_copy_from in dirs_to_copy_from:
    
    if dir_outputs not in dir_to_copy_from:
        os.system('mkdir ' + dir_outputs + '/' + dir_to_copy_from)

        if copy_ins_files:
            os.system('cp ./' + dir_to_copy_from + '/' + '*.ins ' + dir_outputs + '/' + dir_to_copy_from + '/')

        for file_to_copy in files_to_copy:
            print('Now copying ' + dir_to_copy_from + '/all_outputs/' + file_to_copy) 
            os.system('cp ./' + dir_to_copy_from + '/all_outputs/' + file_to_copy + ' ' + dir_outputs + '/' + dir_to_copy_from + '/' + file_to_copy)

make_tar = True
if make_tar:
    os.system('tar -cvf ' + dir_outputs + '.tar ' + dir_outputs)
    os.system('mv ' + dir_outputs + '.tar ~')
    os.system('rm -rf ' + dir_outputs)
