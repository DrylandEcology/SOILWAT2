#!/bin/bash

usage="
Collapse individual log files into a combined file

    * The command searches for all files that match one of the patterns
      'logfile.*' or 'rank_*_logfile.*' in the specified directory.
    * The individual log files are concatenated and the combined content is
      written to the new output file in the specified directory.
    * If the new combined file was created successfully, then the individual
      files are removed.

Usage: ./collapseLogfiles.sh [OPTIONS]

Options:
    -d, --directory       Path to the SOILWAT2 'logs' directory
                          (default is '../logs').

    -o, --output          Name of the new file with the combined logs
                          (default is 'logfile-combined.txt').

    -h, --help            Display this help page.

Example:
./collapseLogfiles.sh
./collapseLogfiles.sh -d ../logs -o logfile-combined.txt
./collapseLogfiles.sh -o $(date +%Y%m%d-%H%M%S)_logfile-combined.txt
"


#--- Defaults
pathLog="../logs"
nameNewLogfile="logfile-combined.txt"

patternLog1="logfile.*"
patternLog2="rank_*_logfile.*"


#--- Command line arguments
while [ $# -gt 0 ]; do
    case $1 in
        -d|--directory) pathLog="$2"; shift ;;

        -o|--output) nameNewLogfile="$2"; shift ;;

        -h|--help) echo "$usage"; exit 0 ;;

        *) echo "Option ""$1"" is not implemented."; exit 1 ;;
    esac
    shift
done


#--- Concatenate individual logfiles from processes into one logfile
newLogfile="${pathLog}/${nameNewLogfile}"

find "${pathLog}" -maxdepth 1 -type f \( -name "${patternLog1}" -or -name "${patternLog2}" \) -print0 |
    sort -zV |
    while IFS= read -r -d '' f; do
        echo "====== ${f#./} ======"
        cat "$f"
        echo
    done > "${newLogfile}"


#--- Remove individual logfiles if concatenation was successful
if [[ "${PIPESTATUS[@]}" =~ [^0\ ] ]]; then
    echo "collapseLogfiles.sh encountered errors."

elif [ -f "${newLogfile}" ]; then
    find "${pathLog}" -maxdepth 1 -type f \( -name "${patternLog1}" -or  -name "${patternLog2}" \) -delete
fi
