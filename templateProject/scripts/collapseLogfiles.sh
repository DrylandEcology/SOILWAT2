#!/bin/bash

pathLog="../logs"
patternLog="*logfile.*"
newLogfile="${pathLog}/logfile-combined.txt"


# Concatenate individual logfiles from IO- and COMP-processes into one logfile
find "${pathLog}" -maxdepth 1 \( -name "${patternLog}" \) -print0 |
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
    find "${pathLog}" -type f \( -name "*_IO_logfile.txt" -or -name "*_COMP_logfile.txt" \) -delete
fi
