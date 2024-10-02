# LPJ-GUESS
## About

Github repository for getting started with LPJ-GUESS (V4) on UC Merced HPC clusters, based on earlier modifications to that code by Weichao Guo/Kristen Emmett and more recent modifications by Philip Myint (pmyint@ucmerced.edu). The repository was created on 2024/08/18 (August 18, 2024) by Philip Myint. This date will be referred to below as the "creation date".

## Getting Started

1. Type `module load mpich` on the command line or, better yet, add this line to your .bashrc (or .cshrc, etc.) file. This will load the MPI module needed to run LPJ-GUESS in parallel. Other MPI modules that could be tried are `mvapich` and `openmpi`.
2. Install Anaconda. As of the creation date, this can be done by going to this website (https://www.anaconda.com/download) and downloading `Anaconda3-2024.06-1-Linux-x86_64.sh` install script. Upload this script to the Merced cluster (or perhaps download it directly through the `wget` command) and execute it. In order to execute it, you may first have to change the script to be recognized as an executable by typing `chmod 770 Anaconda3-2024.06-1-Linux-x86_64.sh`.
3. Go to the Anaconda folder after installation finishes, and install the `netcdf4` package by typing `conda install anaconda::netcdf4` (see https://anaconda.org/anaconda/netcdf4). This package is used by LPJ-GUESS for input/output (I/O) of certain types of environmental/climate data.
4. Make sure CMake is available by typing `cmake --version` into the command line and ensuring that this query does not return an empty string. As of the creation date, CMake version 3.26.5 is available by default in the Merced clusters, which is sufficient, so no action may be necessary for this step.

## Building LPJ-GUESS

1. Clone the repository by typing `git clone https://github.com/philipmyint/LPJ-GUESS.git`. As of the creation date, this will create this README file, plus the following three directories:
	- `src`: the source code
	- `simulations`: directory where simulations will be run. There are currently two subdirectories, `American_River` and `Sierra_Nevada`. The former involves jobs that are run on just 1 node (though it uses 40 cores on that node) and the latter runs a job on 4 nodes (with 24 cores per node since most of the Merced HPC cluster nodes have that many cores per node) for a total of 96 cores.
	- `references`: contains papers and other documents describing the LPJ-GUESS code and some of its modules, including the LMfireCF module.
2. Go to `src` and modify the `set(CONDA_PATH "/home/pmyint/anaconda3")` line in CMakeLists.txt to the path of your Anaconda directory.
3. While in`src`, create a build directory and enter it. This is the directory where the executable will be located.
	- `$ mkdir build`
	- `$ cd build`
4. From the build directory, type `cmake ..`
	- The `..` points `cmake` to the root directory of the project, where the root CMakeLists.txt is located.
	- CMake will configure the project and generate all of the needed build files automatically.
	- If CMake runs into an issue, it will tell you what went wrong. Fix it, then try again.
5. Still within the build directory, run `make -j 20`, which will run make in parallel using 20 threads (can alternatively use any other number, but 20 works pretty well).
6. If the build was successful, an executable `guess` should appear in the `build` directory. It should also report successful pass of all unit tests. As of the creation date, there are 156 unit tests.

## Running LPJ-GUESS

Go to wherever your simulation directories are located (e.g., `simulations/American_River` or `simulations/Sierra_Nevada`), modify the job submission script `submit.sh` and input files as necessary, and submit the job by typing `sbatch submit.sh`. 

### Important: 
The examples are set up assuming that all the input `.ins` instruction files (`main.ins`, `europe.ins`, `landcover.ins`), as well as the gridlist input file (`gridlist_American_River.txt` or `gridlist_Sierra_Nevada.txt`) are located in the same directory where the job will be run. The actual input data (climate forcings, CO2 levels, soil data) are too large to be stored on the Github repository, but they can be obtained from `data/tmpshare/Philip/LPJ-GUESS_inputs` on the cwis server. The examples assume that these inputs are placed in a separate subdirectory called `inputs` of the root `LPJ-GUESS` directory. There are two sets of climate forcings: one from the Coupled Model Intercomparison Project Phase 5 (CMIP5) and a newer dataset from CMIP6. The CMIP5 set has been used by Weichao Guo et al. and involves latitude/longitude values that go to 2 places after the decimal point (e.g., longitude = -122.52), while the newer CMIP6 dataset was recently obtained by Philip Myint and Saswata Nandi and have latitude/longitude values that go to 3 places after the decimal point (e.g., longitude = -122.522). Therefore, make sure to set `latlon_digits = 3` in `landcover.ins` if you are using CMIP6. Note that if `latlon_digits` is unspecified or is set to anything other than 3, the code assumes that there will be 2 digits after the decimal.

#### Other notes:
- The `submit.sh` script will partition the set of gridlist coordinates into a set of `TOTAL_TASKS` coordinates (where `TOTAL_TASKS` = 40 for the `American_River` example and = 96 for the `Sierra_Nevada` example). Each of these smaller gridlist files will be put into a separate `run` subdirectory that is labeled with the corresponding task (MPI process) number.
- The last part of `submit.sh` collects the different `run` subdirectory outputs and merges each output into a single file that it places into another subdirectory called `all_outputs`. For convenience, the example directories also include a `combine_all_outputs.sh` script that performs just this last collection/merge step. There is also a `make_clean.sh` script that removes all the `run` subdirectories, as well as the output log file, and keeps only the `all_outputs` subdirectory. There is also some useful Python scripts, including a script that can be used to copy the input files from one subdirectory to a set of other subdirectories, and another script that can be used to collect outputs on carbon pools, carbon fluxes, and water fluxes (plus other types of outputs as desired) from a set of subdirectories and puts them in a .tar file for easy transfer to, for example, your local machine. 
