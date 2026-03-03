#!/bin/bash

#------ . ------
usage="
Check netCDF files for CF compliance using the IOOS compliance checker

Usage: tools/complianceChecker.sh [OPTIONS]

Options:
    -c, --cf              Version of the CF convention (default is '1.10').
    --cf=<...>

    -d, --directory       Path to directory with netCDF file(s) to check
    --directory=<...>     (default is 'tests/example/Output').

    -i, --input           Full file path and name to a netCDF file to check;
    --input=<...>         overwrites the --directory option (no default).

    -r, --report          Name of the report output (or '-' for stdout)
    --report=<...>        (default is '-').

    -f, --format          Output format of the report (default is 'text'),
    --format=<...>        possible values {text,html,json,json_new}.

    --skipChecks          Skip a check (default ''), e.g.,
    --skipChecks=<...>    check_dimension_order.

    -h, --help            Display this help page.

Examples:
    tools/complianceChecker.sh
    tools/complianceChecker.sh -f text
    tools/complianceChecker.sh -r complianceCheckerReport
    tools/complianceChecker.sh -d tests/example/Output
    tools/complianceChecker.sh -i tests/example/Output/AET_1980-1999_day.nc
    tools/complianceChecker.sh --skipChecks check_dimension_order

Note: See the help page of the compliance-checker for additional information \
including implemented CF versions and report formats.

Note: This script expects that the compliance-checker \
(https://github.com/ioos/compliance-checker) is available on PATH or \
that it is installed in a conda environment named 'ioos'.
"
#------ . ------


#------ Default values ------
ncFileInput=""
pathNCOutputs="tests/example/Output"
reportName="-"
reportFormat="text"
cf="1.10"
skipChecks=""


#------ Command line arguments ------
while [ $# -gt 0 ]; do
    case $1 in
        --cf=*) cf="${1#*=}" ;;
        -c|--cf) cf="$2"; shift ;;

        --directory=*) pathNCOutputs="${1#*=}" ;;
        -d|--directory) pathNCOutputs="$2"; shift ;;

        --input=*) ncFileInput="${1#*=}" ;;
        -i|--input) ncFileInput="$2"; shift ;;

        --report=*) reportName="${1#*=}" ;;
        -r|--report) reportName="$2"; shift ;;

        --format=*) reportFormat="${1#*=}" ;;
        -f|--format) reportFormat="$2"; shift ;;

        --skipChecks=*) skipChecks="${1#*=}" ;;
        --skipChecks) skipChecks="$2"; shift ;;

        -h|--help) echo "$usage"; exit 0 ;;

        *) echo "Option \"$1\" is not implemented."; exit 1 ;;
    esac
    shift
done


case "${reportFormat}" in
    text) outputFormat="txt" ;;
    html) outputFormat="html" ;;
    json|json_new) outputFormat="json" ;;
    *) outputFormat="${reportFormat}"
esac

if [ "${reportName}" = "-" ]; then
    outputReport="-"    # stdout
    reportFormat="text"
else
    outputReport="${reportName}.${outputFormat}"
fi


#------ Locate netCDF file(s) ------

if [ -n "${ncFileInput}" ]; then
    if [ ! -f "${ncFileInput}" ]; then
        echo "'${ncFileInput}' does not exist."
        exit 1
    fi
    ncFileNames=("${ncFileInput}")
else
    ncFileNames=("${pathNCOutputs}/"*".nc")   # Array with full paths
fi


if [ "${#ncFileNames[@]}" -eq 0 ]; then
    echo "No netCDF files found at '${pathNCOutputs}'"
    exit 1
fi


#------ Run the checker ------
loadCONDA=false

if ! command -v compliance-checker > /dev/null 2>&1 ; then
    loadCONDA=true
    source activate
    conda activate ioos
fi

compliance-checker \
    --test=cf:"${cf}" \
    --criteria strict \
    --skip-checks "${skipChecks}" \
    --format "${reportFormat}" \
    --output "${outputReport}" \
    "${ncFileNames[@]}"



if [ "${loadCONDA}" = "true" ]; then
    conda deactivate
fi

unset ncFileNames
#------ . ------

