import os

dirs_to_copy_from = ['CMIP6']
dirs_to_copy_to = ['CMIP5']

dir_current = os.getcwd()

for dir_to_copy_from in dirs_to_copy_from:
    for dir_to_copy_to in dirs_to_copy_to:
        os.system('cp ' + dir_to_copy_from + '/*.ins ' + dir_to_copy_from + '/gridlist* ' + dir_to_copy_from + '/*.sh ' + dir_to_copy_to)

        # Change to the directory and run the make_clean script to make sure that we have only input and script files in that directory
        os.chdir(dir_to_copy_to)
        os.system('echo $PWD')
        os.system('./make_clean.sh')
        os.chdir(dir_current)
