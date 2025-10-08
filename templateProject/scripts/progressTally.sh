#!/bin/bash

usage="
Tally progress of a nc-based SOILWAT2 simulation

    * The command searches for a 'progress*.nc' file.
    * 'cdo' is used to tally the following
        * total number of simulation units ('suids'),
        * number of completed simulation units, and
        * number of failed simulation units.
    * A table with the tallies and percentages of total is printed.

Usage: ./progressTally.sh [OPTIONS]

Options:
    -d, --directory     Path to the 'Input_nc/' directory
                        (default is '../Input_nc/').

    -h, --help          Display this help page.

Example:
./progressTally.sh
|          |   #suids | #success |  #failed |
| -------- | -------- | -------- | -------- |
|    Count |   112138 |    47465 |      200 |
|  Percent |   100.00 |    42.33 |     0.18 |
"


#--- Command line arguments
dirInputNC="../Input_nc"

while [ $# -gt 0 ]; do
    case $1 in
        -d|--directory) dirInputNC="$2"; shift ;;

        -h|--help) echo "$usage"; exit 0 ;;

        *) echo "Option ""$1"" is not implemented."; exit 1 ;;
    esac
    shift
done


#--- Check that cdo is available
if ! command -v cdo > /dev/null 2>&1 ; then
    echo "Error: 'cdo' is not available."
    exit 1
fi


#--- Path and file name to progress.nc
fname_progress=$(find "${dirInputNC}" -type f -iname "progress*.nc" | head -n 1)

if [ ! -f "${fname_progress}" ]; then
    echo "Error: progress.nc not found."
    exit 1
fi


#--- Copy progress.nc to temporary file
# cdo cannot read the progress.nc if it is being used by SOILWAT2
ftmp="${fname_progress##*/}"
fname_tmp="${fname_progress%/*}"/"${ftmp/.nc/_tmp.nc}"

if [ -f "${fname_tmp}" ]; then
    rm "${fname_tmp}"
fi
cp "${fname_progress}" "${fname_tmp}"


#--- Tally progress
nCompleted=$(cdo -L -b f64 -s -output -fldcount -selname,progress "${fname_tmp}")
status=$?

if [ $status -ne 0 ]; then
    echo "Error: 'cdo' cannot read the progresss.nc file."
    rm "${fname_tmp}"
    exit 1
fi

nSuccess=$(cdo -L -b f64 -s -output -fldsum -gtc,0 -selname,progress "${fname_tmp}")
pctSuccess=$(echo "100 * ${nSuccess} / ${nCompleted}" | bc -l)

nFailed=$(cdo -L -b f64 -s -output -fldsum -ltc,0 -selname,progress "${fname_tmp}")
pctFailed=$(echo "100 * ${nFailed} / ${nCompleted}" | bc -l)


#--- Print progress
cellWidth=8
cellSep=$(printf '%0.s-' $(seq 1 ${cellWidth}))
rowFormatString="| %${cellWidth}.${cellWidth}s | %${cellWidth}.${cellWidth}s | %${cellWidth}.${cellWidth}s | %${cellWidth}.${cellWidth}s |\n"
rowFormatInt="| %${cellWidth}.${cellWidth}s | %${cellWidth}d | %${cellWidth}d | %${cellWidth}d |\n"
rowFormatFloat="| %${cellWidth}.${cellWidth}s | %${cellWidth}.2f | %${cellWidth}.2f | %${cellWidth}.2f |\n"

printf "${rowFormatString}" "" "#suids" "#success" "#failed"
printf "${rowFormatString}" "${cellSep}" "${cellSep}" "${cellSep}" "${cellSep}"
printf "${rowFormatInt}" "Count" ${nCompleted} ${nSuccess} ${nFailed}
printf "${rowFormatFloat}" "Percent" 100 ${pctSuccess} ${pctFailed}


#--- Cleanup
rm "${fname_tmp}"
