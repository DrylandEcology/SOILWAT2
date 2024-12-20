#!/bin/bash

# usage get_macav2metdata.sh [-o|--out <path/to/data>]

dir_out="data"
url_server="http://thredds.northwestknowledge.net:8080/thredds/ncss/grid/MACAV2"
gcm="HadGEM2-ES365"
exp="historical"
realization="r1i1p1"
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


declare -a years=("1980_1984" "1985_1989" "1990_1994" "1995_1999" "2000_2004" "2005_2005")

declare -a vars=(
    "pr"
    "rhsmax"
    "rhsmin"
    "rsds"
    "tasmin"
    "tasmax"
    "uas"
    "vas"
)

nvars=${#vars[@]}


for ((kv = 0; kv < nvars; kv++)); do
    subset_query="&var=all&north=""${bbNorth}""&south=""${bbSouth}""&west=""${bbWest}""&east=""${bbEast}""&temporal=all&accept=netcdf&point=false"

    for yr in "${years[@]}"; do
        fname=macav2metdata_"${vars[kv]}"_"${gcm}"_"${realization}"_"${exp}"_"${yr}"_CONUS_daily".nc"
        url_file="${url_server}"/"${gcm}"/"${fname}""?""${subset_query}"

        if [ ! -f "${dir_out}"/"${fname}" ]; then
            wget -nc -c -nv -nd -O "${dir_out}"/"${fname}" "${url_file}"
            sleep 1
        fi
    done
done

unset -v years
unset -v vars
