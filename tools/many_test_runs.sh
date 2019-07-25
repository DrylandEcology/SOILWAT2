#!/bin/bash

# N=3 ./tools/many_test_runs.sh: run unit tests repeatedly and report failures
# $N number of test runs; default 10 if empty or unset

iters=${N:-10}
sw_test=${sw_test:-"sw_test"}

echo $(date): will run ${iters} test replicates.

if [ ! -x sw_test ]; then
  make test
fi

for i in $(seq 1 $iters); do
  temp=$( ./sw_test | grep -i "Fail" )

  if [ ! -z "${temp}" ]; then
    echo Replicate $i failed with:
    echo ${temp}
  fi
done

echo $(date): Completed ${iters} test replicates.
