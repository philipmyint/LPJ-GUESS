#!/bin/bash
TOTAL_TASKS=40

#set -e

mkdir -p all_output

function append_files {
    local number_of_jobs=$1
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

mv *.out ./all_output/
