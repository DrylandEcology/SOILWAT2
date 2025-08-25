#!/bin/bash

find ../logs/ -maxdepth 1 \( -name "*logfile.*" \) -print0 |
    sort -zV |
    while IFS= read -r -d '' f; do
        echo "====== ${f#./} ======"
        cat "$f"
        echo
    done > ../logs/logfile-combined.txt

