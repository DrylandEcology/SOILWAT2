#!/bin/sh

if [ ! -d "../logs" ]; then
    mkdir ../logs
fi

# Run project with nc-based SOILWAT2
date

../SOILWAT2 -d ../ -f files.txt \
    > ../logs/$(date +%Y%m%d-%H%M%S)_log-sw2_local.txt 2>&1

./progressTally.sh

date
echo "End of file."
