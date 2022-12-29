#!/bin/bash

# N=3 ./tools/many_test_runs.sh: run unit tests repeatedly and report failures
# $N number of test runs; default 10 if empty or unset

iters=${N:-10}

echo $(date): will run ${iters} test replicates.

make test

for i in $(seq 1 $iters); do
  temp=$( make test_run | grep -i "Fail" )

  if [ ! -z "${temp}" ]; then
    echo Replicate $i failed with:
    echo ${temp}
  fi
done

echo $(date): Completed ${iters} test replicates.
