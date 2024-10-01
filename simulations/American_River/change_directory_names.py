import os

dir_current = os.getcwd()

for directory in [a for a in os.listdir(dir_current) if os.path.isdir(a)]:

    if 'no_treatment' in directory:
        print(f'Changing name of {directory}/')
        os.system('mv ' + directory + ' no_management' + directory[12:])
