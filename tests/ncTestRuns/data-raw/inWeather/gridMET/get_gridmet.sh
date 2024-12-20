#!/bin/bash

# usage get_gridmet.sh [-o|--out <path/to/data>]

dir_out="data"
url_server="http://thredds.northwestknowledge.net:8080/thredds/ncss/MET"
bbNorth="39.5939"
bbSouth="39.5784"
bbWest="-105.5909"
bbEast="-105.5691"


#--- Command line arguments
while [ $# -gt 0 ]; do
    case $1 in
        -o|--out) dir_out="$2"; shift ;;

        *) echo "Option ""$1"" is not implemented"; exit 1 ;;
    esac
    shift
done


if [ ! -d "${dir_out}" ]; then
    mkdir "${dir_out}"
fi


years=({1980..2010})

declare -a vars=(
    "pr"
    "rmax"
    "rmin"
    "srad"
    "tmmn"
    "tmmx"
    "vs"
)

nvars=${#vars[@]}


for ((kv = 0; kv < nvars; kv++)); do
    for yr in "${years[@]}"; do
        fname="${vars[kv]}"_"${yr}"".nc"
        subset_query="var=all&north=""${bbNorth}""&west=""${bbWest}""&east=""${bbEast}""&south=""${bbSouth}""&disableProjSubset=on&horizStride=1&temporal=all&timeStride=1&accept=netcdf"
        url_file="${url_server}"/"${vars[kv]}"/"${fname}""?""${subset_query}"

        if [ ! -f "${dir_out}"/"${fname}" ]; then
            wget -nc -c -nv -nd -O "${dir_out}"/"${fname}" "${url_file}"
            sleep 1
        fi
    done
done

unset -v vars

