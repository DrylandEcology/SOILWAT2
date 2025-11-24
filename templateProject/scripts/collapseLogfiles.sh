#!/bin/bash

pathLog="../logs"
patternLog1="logfile.*"
patternLog2="rank_*_logfile.*"
newLogfile="${pathLog}/logfile-combined.txt"


# Concatenate individual logfiles from processes into one logfile
find "${pathLog}" -type f -maxdepth 1 \( -name "${patternLog1}" -or -name "${patternLog2}" \) -print0 |
    sort -zV |
    while IFS= read -r -d '' f; do
        echo "====== ${f#./} ======"
        cat "$f"
        echo
    done > "${newLogfile}"


# Remove individual logfiles if concatenation was successful
if [[ "${PIPESTATUS[@]}" =~ [^0\ ] ]]; then
    echo "collapseLogfiles.sh encountered errors."

elif [ -f "${newLogfile}" ]; then
    find "${pathLog}" -type f -maxdepth 1 \( -name "${patternLog1}" -or  -name "${patternLog2}" \) -delete
fi
