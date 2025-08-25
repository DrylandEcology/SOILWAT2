#!/bin/bash

usage="
Standardize netCDF files created by SOILWAT2

    * The command searches for all '*.nc' files in the specified directory.
    * nco's ncpdq command is used to
        * permutate from a time-before-space to a space-before-time organization
        * chunk suitably for space-before-time
        * compress the file (deflate)
    * Add attribute '_FillValue' if missing from variables.

Usage: ./ncSW2PermuteStandard.sh [OPTIONS]

Options:
    -d, --directory       Path to the SOILWAT2 'Output' directory
                          (default is '../Output').

    -d2, --directory-out  Path to the directory where the reformatted output
                          is written to (default is '../Output',
                          i.e., overwritting existing files).

    -x, --longitude       Name of the x-axis (default is 'longitude').

    -y, --latitude        Name of the y-axis (default is 'latitude').

    -z, --vertical        Name of the vertical axis (default is 'vertical').

    -t, --time            Name of the time axis (default is 'time').

    -c, --chunk_cache     Cache size for chunking (GB, default is '2').

    -l, --deflate_level   Deflate level for compression (default is '5').

    -h, --help            Display this help page.

Example:
./ncSW2PermutateStandard.sh -d ../Output
"


#--- Defaults
dirOutputSW1="../Output"
dirOutputSW2="../Output"

deflateLevel=5

nameAxisT="time"
nameAxisX="longitude"
nameAxisY="latitude"
nameAxisZ="vertical"
nameAxisPFT="pft"

chunkCacheGB=2

NC_FILL_DOUBLE=9.9692099683868690e+36


#--- Command line arguments
while [ $# -gt 0 ]; do
    case $1 in
        -d|--directory) dirOutputSW1="$2"; shift ;;

        -d2|--directory-out) dirOutputSW2="$2"; shift ;;

        -x|--longitude) nameAxisX="$2"; shift ;;

        -y|--latitude) nameAxisY="$2"; shift ;;

        -z|--vertical) nameAxisZ="$2"; shift ;;

        -t|--time) nameAxisT="$2"; shift ;;

        -c|--chunk_cache) chunkCacheGB="$2"; shift ;;

        -l|--deflate_level) deflateLevel="$2"; shift ;;

        -h|--help) echo "$usage"; exit 0 ;;

        *) echo "Option ""$1"" is not implemented."; exit 1 ;;
    esac
    shift
done


#--- Check that dependencies are available
if ! command -v ncpdq > /dev/null 2>&1 ; then
    echo "Error: 'nco' is not available."
    exit 1
fi

if ! command -v ncdump > /dev/null 2>&1 ; then
    echo "Error: 'ncdump' is not available."
    exit 1
fi


#--- Functions
progressBar() {
    local current=$1
    local total=$2
    local width=50

    local pct=$(( 100 * current / total ))
    local filled=$(( width * current / total ))

    local bar=""
    for (( i=0; i<filled; i++ )); do
        bar="$bar#"
    done
    for (( i=filled; i<width; i++ )); do
        bar="$bar-"
    done

    printf "\r[%s] %d%%" "$bar" "$pct"
}


#--- Create directory for output
if [[ ! -d "${dirOutputSW2}" ]]; then
    mkdir "${dirOutputSW2}"
fi



#--- Make SOILWAT2 output suitable
nTotal=$(find "${dirOutputSW1}" -maxdepth 1 -name "*.nc" -print0 | grep -zc '' )
kProgress=0
progressBar $kProgress $nTotal

find "${dirOutputSW1}" -maxdepth 1 -name "*.nc" -print0 | while IFS= read -r -d $'\0' fileSW2; do

    filenameSW2="${fileSW2##*/}" # basename
    fileResult="${dirOutputSW2}/${filenameSW2}"
    fileTmp="${dirOutputSW2}/tmp__${filenameSW2}"


    if [[ ! -f "${fileResult}" ]]; then

        #--- Locate coordinate axes
        dimensions=$(ncdump -h "${fileSW2}" | awk '/^dimensions:/, /^variables:/' | grep '=' | awk '{gsub(/;/, "", $1); print $1}')

        permutation=""
        for var in "${nameAxisT}" "${nameAxisZ}" "${nameAxisY}" "${nameAxisX}"; do
            if echo "$dimensions" | grep -q "\b$var\b"; then
                permutation+="$var,"
            fi
        done
        permutation="${permutation%,}"


        #--- Permutate axes and first part of chunking
        # bc v1.07.1 does not support exponential notation -> use printf
        chunkCacheB=$(echo "${chunkCacheGB} * $(printf "%.0f" 1e9) / 1" | bc)

        ncpdq --fl_fmt=netcdf4 \
            --permute="${permutation}" \
            --deflate=${deflateLevel} \
            --chunk_cache="${chunkCacheB}" --cnk_dmn="${nameAxisT}",365 --cnk_dmn="${nameAxisY}",100 --cnk_dmn="${nameAxisX}",100 \
            "${fileSW2}" "${fileTmp}"


        if [[ -f "${fileTmp}" ]]; then
            #--- Deflate and final part of chunking
            # Note: --permute option passed again to avoid packing
            ncpdq --fl_fmt=netcdf4 \
                --permute="${permutation}" \
                --deflate=${deflateLevel} \
                --chunk_cache="${chunkCacheB}" --cnk_dmn="${nameAxisT}",1 \
                "${fileTmp}" "${fileResult}"

            rm "${fileTmp}"

        else
            echo "Error: Permutation failed for file '${fileSW2}'."
            exit 1
        fi
    fi


    #--- Add _FillValue attributes if missing
    dump=$(ncdump -h "${fileResult}")

    # Get variable names from, e.g., "  double temperature(time, lat, lon) ;"
    vars=$(echo "${dump}" | \
        awk '/^[[:space:]]*double[[:space:]]+[a-zA-Z0-9_]*\(.*\)/ {
            split($2, var, "(");
            print var[1];
        }'
    )

    dimensions=$(echo "${dump}" | awk '/^dimensions:/, /^variables:/' | grep '=' | awk '{gsub(/;/, "", $1); print $1}')
    dimensions="${dimensions} _bnds"

    for var in $vars; do
        if ! echo "${dump}" | grep -q "${var}:_FillValue" && [[ ! "${dimensions}" =~ (^|[[:space:]])$var($|[[:space:]]) ]] && [[ ! "$var" == *_bnds ]]; then
            echo "Adding _FillValue attribute to variable ${var} in file ${filenameSW2}"
            ncatted -h -a _FillValue,"${var}",c,d,"${NC_FILL_DOUBLE}" "${fileResult}"
        fi
    done


    #--- Report progress
    ((kProgress++))
    progressBar $kProgress $nTotal
done

echo "" # Complete progress bar
