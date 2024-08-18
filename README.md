# LPJ-GUESS
## About

Repository for getting started with LPJ-GUESS on UC Merced HPC clusters, based on earlier modifications to that code from Weichao Guo. Contact info: Philip Myint (pmyint@ucmerced.edu). Date that this README was created: 2024/08/18. This date will be referred to below as the "current date".

## Getting Started

1. Type `module load mpich` on the command line or add this to your .bashrc (or .cshrc, etc.) file. This will load the MPI module needed to run LPJ-GUESS in parallel. Other MPI modules that could be tried are `mvapich` and `openmpi`.
2. Install Anaconda. As of the current date, this can be done by going to this website (https://www.anaconda.com/download), entering your email, and downloading `Anaconda3-2024.06-1-Linux-x86_64.sh` install script. Upload this script to the Merced cluster and execute it. In order to execute it, you may first have to change the script to be recognized as an executable by typing `chmod 770 Anaconda3-2024.06-1-Linux-x86_64.sh`.
3. Go to the Anaconda folder after installation finishes, and install the `netcdf4` package by typing `conda install anaconda::netcdf4` (see https://anaconda.org/anaconda/netcdf4). This package is used by LPJ-GUESS for input/output (I/O) of certain types of environmental/climate data.
4. Make sure CMake is available by typing `cmake --version` into the command line and ensuring that this query does not return an empty string. As of the current date, CMake version 3.26.5 is available by default in the Merced clusters, which is sufficient, so no action is necessary for this step.

## Building LPJ-GUESS

1. Clone the repository by typing `git clone https://github.com/philipmyint/LPJ-GUESS.git`. As of the current date, this will create this README file, plus the following three directories:
	- `src_LPJ_GUESS`: the source code
	- `simulations_LPJ_GUESS`: directory where simulations will be run. There are currently two subdirectories, `test` and `Sierra_Nevada`. The former runs a job on just 1 node 	      (though it uses 40 cores on that node) and the latter runs a job on 4 nodes (again with 40 cores per node) for a total of 160 cores.
	- `data_LPJ_GUESS`: contains input files and other types of data 
2. Go to `src_LPJ_GUESS` and modify the `set(CONDA_PATH "/home/pmyint/anaconda3")` line in CMakeLists.txt to the path of your Anaconda directory.
3. While in`src_LPJ_GUESS`, create a build directory and enter it. This is the directory where the executable will be located.
    	- `$ mkdir build`
    	- `$ cd build`
4. From the build directory, type `cmake ..`
    	- The `..` points `cmake` to the root directory of the project, where the root CMakeLists.txt is located.
    	- CMake will configure the project and generate all of the needed build files automatically.
    	- If CMake runs into an issue, it will tell you what went wrong. Fix it, then try again.
5. Still within the build directory, run `make -j 20`, which will run make in parallel using 20 threads (can also use any number other than 20).
6. If the build was successful, an executable `guess` should appear in the `build` directory. It should also report successful pass of all unit tests. As of the current date, there are 156 unit tests.

## Running LPJ-GUESS

Go to wherever your simulation directories are (e.g., `simulations_LPJ_GUESS/test` or `simulations_LPJ_GUESS/Sierra_Nevada`), modify `submit.sh` and other files as necessary, and submit the job by typing `sbatch submit.sh`. The examples are set up assuming that all the `.ins` input files (e.g., `Sierra_Nevada.ins`, `europe.ins`, `landcover.ins`), as well as the gridlist input file (e.g., `gridlist_sn.txt`) are located in the same directory where the job will be run.
1. The `submit.sh` script will partition the set of gridlist coordinates into a set of TOTAL_TASKS (=40 for `test` and =160 for `Sierra_Nevada` examples) coordinates. Each 
of these smaller gridlist_files will be put into a separate `run` directory that is labeled with the task (MPI process) number.
2. The last part of `submit.sh` collects/merges the different `run` outputs and into a single directory `all_output`. For convenience, the example directories also include a	    `combine_all_outputs.sh` script that does just this last collection/merge step. There is also a `make_clean.sh` script that removes all the `run` subdirectories and keeps 
only the `all_outputs` subdirectory.
