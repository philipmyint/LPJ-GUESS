# LPJ-GUESS
## About

Repository for getting started with LPJ-GUESS on UC Merced HPC clusters, based on earlier modifications to that code from Weichao Guo. Contact info: Philip Myint (pmyint@ucmerced.edu). Date that this README was created: 2024/08/18. This date will be referred to below as the "current date."

## Getting Started

1. Type `module load mpich` on the command line or add this to your .bashrc (or .cshrc, etc.) file. This will load the MPI module needed to run LPJ-GUESS in parallel.
2. Install Anaconda. As of the current date, this can be done by going to this website (https://www.anaconda.com/download), entering your email, and downloading Anaconda3-2024.06-1-Linux-x86_64.sh install script. Upload this script to the Merced cluster and execute it. In order to execute it, you may first have to change the script to be recognized as an executable by typing `chmod 770 Anaconda3-2024.06-1-Linux-x86_64.sh`.
3. Go to the Anaconda folder after installation finishes, and install the `netcdf4` package by typing `conda install anaconda::netcdf4` (see https://anaconda.org/anaconda/netcdf4). This package is used by LPJ-GUESS for input/output (I/O) of certain types of environmental/climate data.

## Building LPJ-GUESS


