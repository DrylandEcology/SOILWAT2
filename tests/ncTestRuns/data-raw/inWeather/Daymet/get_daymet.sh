#!/bin/bash

# usage get_daymet.sh [-o|--out <path/to/data>]

dir_out="data"
url_server="https://thredds.daac.ornl.gov/thredds/ncss/ornldaac/2129"
bbNorth="39.59385433645"
bbSouth="39.57843699065"
bbWest="-105.5909398144"
bbEast="-105.5690601856"


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


# precipitation
# solar radiation
# min air temperature
# max air temperature
# vapor pressure
declare -a vars=("prcp" "srad" "tmin" "tmax" "vp")

nvars=${#vars[@]}


for ((kv = 0; kv < nvars; kv++)); do
    for yr in "${years[@]}"; do
        fname=daymet_v4_daily_na_"${vars[kv]}"_"${yr}"".nc"
        subset_query="var=lat&var=lon&var=""${vars[kv]}""&north=""${bbNorth}""&west=""${bbWest}""&east=""${bbEast}""&south=""${bbSouth}""&disableProjSubset=on&horizStride=1&temporal=all&timeStride=1&accept=netcdf"
        url_file="${url_server}"/"${fname}""?""${subset_query}"

        if [ ! -f "${dir_out}"/"${fname}" ]; then
            wget -nc -c -nv -nd -O "${dir_out}"/"${fname}" "${url_file}"
            sleep 1
        fi
    done
done

unset -v vars
