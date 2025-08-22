#!/bin/bash

usage="
Aggregate and concatenate a variable

    * The output can be concatenated along time into one file.

    * 'cdo' is used to aggregate from daily values to the requested
      output time period (month, year) and to concatenate along time.

Usage: ./metric_one.sh [OPTIONS]

Options:
    -d, --directory     Path to the directory that contains SOILWAT2 output
                        (default is '../Output/').

    -d2,                Path to the directory where the resulting metric
    --directory-out     will be saved (default is '../Metrics/').

    -t, --time-step     Time step (day, month, year) over which daily values
                        will be summed (default is 'year').

    -c, --concat-time   Concatenate resulting nc-files across time.

    -s, --summary       Method to summarize values over time, one of
                        'mean', 'sum' (default is 'sum').

    -f1, --fid          File name tag (SOILWAT2 output group name) identifying
                        the nc-file(s) (no default).

    -h, --help          Display this help page.

Example:
./metric_one.sh -t month -c
"


#--- Defaults
dirOutputSW="../Output"
dirMetrics="../Metrics"

fileNameTagInput1=""

outTimeStep="year"
doConcatTS=false
requestedSummary="sum"


#--- Command line arguments
while [ $# -gt 0 ]; do
    case $1 in
        -d|--directory) dirOutputSW="$2"; shift ;;

        -d2|--directory-out) dirMetrics="$2"; shift ;;

        -t|--time-step) outTimeStep="$2"; shift ;;

        -c|--concat-time) doConcatTS=true ;;

        -s|--summary) requestedSummary="$2"; shift ;;

        -f1|--fid) fileNameTagInput1="$2"; shift ;;

        -h|--help) echo "$usage"; exit 0 ;;

        *) echo "Option \"$1\" is not implemented."; exit 1 ;;
    esac
    shift
done


#--- Set output name tag
fileNameTagOut="${fileNameTagInput1}"

if [ -z "${fileNameTagOut}" ]; then
    echo "Error: option '--fid' is empty."
    exit 1
fi


#--- Check that cdo is available
if ! command -v cdo > /dev/null 2>&1 ; then
    echo "Error: 'cdo' is not available."
    exit 1
fi


#--- Create directory for metrics
if [[ ! -d "${dirMetrics}" ]]; then
    mkdir "${dirMetrics}"
fi


#--- Locate files across time slices
ts=$(find "${dirOutputSW}" -maxdepth 1 -name "${fileNameTagInput1}_*_day.nc" | sed -E "s/.*${fileNameTagInput1}_([0-9]{4})(-[0-9]{4})?_day\.nc/\1\2/" | sort | uniq)
timeSlices=($ts) # convert to array using word splitting based on IFS


if [ "${doConcatTS}" = "true" ]; then
    firstYear="${timeSlices[0]}"
    firstYear="${firstYear%%-*}" # get content before "-" or the whole string

    lastYear="${timeSlices[${#timeSlices[@]}-1]}"
    if [[ "${lastYear}" == *"-"* ]]; then
        lastYear="${lastYear##*-}" # get content after the last "-"
    fi

    if [ "${firstYear}" = "${lastYear}" ]; then
        rangeYears="${firstYear}"
    else
        rangeYears="${firstYear}-${lastYear}"
    fi

    fileResultAll="${dirMetrics}/${fileNameTagOut}_${rangeYears}_${outTimeStep}.nc"

    if [[ -f "${fileResultAll}" ]]; then
        exit 0
    fi
fi


#--- Calculate metric for each time slice
anyMissingFiles=false

for timeSlice in "${timeSlices[@]}"; do
    fileInput1="${dirOutputSW}/${fileNameTagInput1}_${timeSlice}_day.nc"

    fileResultDay="${fileInput1}"
    fileResultTS="${dirMetrics}/${fileNameTagOut}_${timeSlice}_${outTimeStep}.nc"

    if [[ ! -f "${fileResultTS}" ]]; then

        if [[ ! -f "${fileResultDay}" ]]; then
            echo "Warning: Missing input file(s) for timeSlice '${timeSlice}'."
            anyMissingFiles=true
        fi


        # Summarize to requested output time step
        case "${outTimeStep}" in
            day) ;;

            month)
                cdo -f nc4 -z zip,5 "mon${requestedSummary}" "${fileResultDay}" "${fileResultTS}"
                ;;

            year)
                cdo -f nc4 -z zip,5 "year${requestedSummary}" "${fileResultDay}" "${fileResultTS}"
                ;;

            *) echo "Time step \"$1\" is not implemented."; exit 1 ;;
        esac
    fi

    if [[ ! -f "${fileResultTS}" ]]; then
        echo "Warning: Unsuccessful metric file '${fileResultTS}'."
        anyMissingFiles=true
    fi
done


#--- Concatenate metric across time slices
if [ "${doConcatTS}" = "true" ]; then

    if [ "${anyMissingFiles}" = "true" ]; then
        echo "Error: Concatenation failed because of missing timeSlices."
        exit 1
    fi

    cdo -f nc4 -z zip,5 cat \
        "${dirMetrics}/${fileNameTagOut}_*_${outTimeStep}.nc" \
        "${fileResultAll}"
fi
