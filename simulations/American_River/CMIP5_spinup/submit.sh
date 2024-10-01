#!/bin/bash  
#SBATCH --job-name=LPJ-GUESS  #the job name
#SBATCH --nodes=1  #number of requested nodes
#SBATCH --ntasks-per-node=40  #tasks/cores on each node
#SBATCH --partition test #choices include test, short, medium, long, bigmem (see https://ucm-it.github.io/hpc_docs/)
#SBATCH --mem=96G  #total memory requested, use 0 if you want to use entire node memory
#SBATCH --time=0-1:00:00 # hh:mm::ss
#SBATCH --output=output_%j.log  #the output file containing information from the standard output
#SBATCH --export=ALL
# #SBATCH -M merced # add this flag if you want to submit jobs to MERCED cluster
#SBATCH --mail-user=pmyint@ucmerced.edu
# #SBATCH --gres=gpu:X # uncomment this line if you need GPU access, replace X with number of GPU you need
# #SBATCH -w <selected_node> #uncomment this line if you want to select specific available node to run 

# TOTAL_TASKS = Number of nodes times the number of tasks per node
TOTAL_TASKS=40
INSFILE=main.ins
INPUT_MODULE=cru_ncep
GRIDLIST=gridlist_American_River.txt
OUTFILES='*.out'
EXEC=/home/pmyint/data/LPJ-GUESS/src/build/guess


# Nothing to change past here
########################################################################

# Exit if any command fails
set -e

# Handle the command line arguments
while getopts ":n:s:i:" opt; do
    case $opt in
        n ) name=$OPTARG ;;
        s ) submit_vars_file=$OPTARG ;;
        i ) ins=$OPTARG ;;
    esac
done

# Override the submit variables with the contents of a file, if given
if [ -n "$submit_vars_file" ]; then
    source $submit_vars_file
fi

# Override INSFILE with the ins-file parameter, if given
if [ -n "$ins" ]; then
    INSFILE=$ins
fi

# Convert INSFILE to an absolute path since we will be starting the
# guess instances from different directories.
# Please note when porting this script: readlink may not be available
# on non-Linux systems. Also, using absolute path names means the
# instruction file needs to be in a place accessible from the nodes.
INSFILE=$(readlink -f "$INSFILE")

GRIDLIST_FILENAME=$(basename $GRIDLIST)

# This function creates the gridlist files for each run by splitting
# the original gridlist file into approximately equal parts.
function split_gridlist {
    # Create empty gridlists first to make sure each run gets one
    for ((a=1; a <= TOTAL_TASKS ; a++))
    do
      echo > run$a/$GRIDLIST_FILENAME
    done

    # Figure out suitable number of lines per gridlist, get the number of
    # lines in original gridlist file, divide by TOTAL_TASKS and round up.
    local lines_per_run=$(wc -l $GRIDLIST | \
        awk '{ x = $1/'$TOTAL_TASKS'; d = (x == int(x)) ? x : int(x)+1; print d}')

    # Use the split command to split the files into temporary files
    split --suffix-length=4 --lines $lines_per_run $GRIDLIST tmpSPLITGRID_

    # Move the temporary files into the runX-directories
    local files=$(ls tmpSPLITGRID_*)
    local i=1
    for file in $files
    do
      mv $file run$i/$GRIDLIST_FILENAME
      i=$((i+1))
    done
}

# Create header of progress.sh script
echo "##############################################################" > progress.sh
echo "# PROGRESS.SH" >> progress.sh
echo "# Upload current guess.log files from local nodes and check" >> progress.sh
echo "# Usage: sh progress.sh" >> progress.sh
echo >> progress.sh

# Create a run subdirectory for each task/core and clean up
for ((a=1; a <= TOTAL_TASKS ; a++))
do
  mkdir -p run$a
  cd run$a ; rm -f guess.log ; rm -f $GRIDLIST_FILENAME ; cd ..
  echo "echo '********** Last few lines of ./run${a}/guess.log: **********'" >> progress.sh
  echo "tail ./run${a}/guess.log" >> progress.sh
done
split_gridlist

# Run the job
echo "Now running simulations ..."
date
mpiexec $EXEC -parallel -input $INPUT_MODULE $INSFILE
echo "Done with running simulations"

# Merge/combine the output from the different tasks/cores into a single directory
echo ""
echo "Now merging the output from the different tasks/cores ..."

#set -e

mkdir -p all_outputs

function append_files {
    local number_of_jobs=$TOTAL_TASKS
    local file=$2

    cp run1/$file $file

    local i=""
    for ((i=2; i <= number_of_jobs; i++))
    do
      if [ -f run$i/$file ]; then
        cat run$i/$file | awk 'NR!=1 || NF==0 || $1 == $1+0 { print $0 }' >> $file
      fi
    done
}

pushd run1 &> /dev/null
outfiles_unexpanded='*.out'
outfiles_expanded=$(echo $outfiles_unexpanded)
popd &> /dev/null

for file in $outfiles_expanded
do
  append_files $TOTAL_TASKS $file
done
#cat run*/guess.log > guess.log

mv *.out ./all_outputs/
echo "Done with merging the output"
