#!/bin/bash

#------ . ------
usage="
Create, execute, and check a set of nc-based SOILWAT2 test runs

    * Comprehensive set of spatial configurations of simulation domains and
      input netCDFs (see 'tests/ncTestRuns/data-raw/metadata_testRuns.csv')
    * (if available) Runs with daily inputs from external data sources
      including Daymet, gridMET, and MACAv2METDATA
    * One site/grid cell in the simulation domain is set up to correspond
      to the reference run (which, by default, is equivalent to tests/example);
      the output of that site/grid cell is compared against the reference output

Usage: tools/check_ncTestRuns.sh [ACTIONS] [OPTIONS]

Actions:
    all                 Combine options 'create' and 'check';
                        'all' is the default if no actions are specified.
    create              Create template for selected test run(s).
    check               Execute SOILWAT2 and check selected test run(s).
    clean               Remove selected test setup and run(s).
    cleanTestRuns       Remove selected test run(s).
    cleanReference      Remove the default reference run.
    downloadExternalWeatherData Runs 'wget' scripts to download daily inputs
                        from external data sources including
                        Daymet, gridMET, and MACAv2METDATA

Options:
    -ref, --reference <path/to/reference/output>
                        Path to reference run output. If the default is
                        selected as reference and output is missing, then
                        SOILWAT2 will be run to create reference output.
                        Default is 'tests/ncTestRuns/results/referenceRun/Output'
    -t, --testRun <test number>
                        Perform action(s) on selected test run(s);
                        default is '-1', i.e., all test runs.
    -h, --help          Display this help page.

Examples:
    * do all tests:
        tools/check_ncTestRuns.sh
    * do all tests and clean first:
        tools/check_ncTestRuns.sh clean all
    * check all tests and clean reference:
        tools/check_ncTestRuns.sh cleanReference check
    * do first test:
        tools/check_ncTestRuns.sh -t 1
    * check second test:
        tools/check_ncTestRuns.sh check -t 2
    * delete third test and reference:
        tools/check_ncTestRuns.sh clean -t 3 cleanReference
"
#------ . ------


#------ SETTINGS ----------------------------------------------------------


#--- Path to ncTestRuns/
dir_ncTestRuns="tests/ncTestRuns"

#--- Name of SOILWAT2 executable
sw2="bin/SOILWAT2"


#--- Command line arguments
doCleanTestRuns=false
doCleanTests=false
doCleanReference=false
doCreate=false
doCheck=false
doExternalDownload=false
dirOutRefDefault="${dir_ncTestRuns}""/results/referenceRun/Output"
dirOutRef="${dirOutRefDefault}"
testRun=-1


while [ $# -gt 0 ]; do
    case $1 in
        cleanTestRuns) doCleanTestRuns=true ;;

        clean) doCleanTests=true ;;

        cleanReference) doCleanReference=true ;;

        create) doCreate=true ;;

        check) doCheck=true ;;

        all) doCreate=true; doCheck=true ;;

        downloadExternalWeatherData) doExternalDownload=true ;;

        -ref|--reference) dirOutRef="$2"; shift ;;

        -t|--testRun) testRun="$2"; shift ;;

        -h|--help) echo "$usage"; exit 0 ;;

        *) echo "Option ""$1"" is not implemented"; exit 1 ;;
    esac
    shift
done


# no action is equivalent to "all"
if [ "${doCleanTestRuns}" = "false" ] && \
    [ "${doCleanTests}" = "false" ] && \
    [ "${doCleanReference}" = "false" ] && \
    [ "${doCreate}" = "false" ] && \
    [ "${doCheck}" = "false" ] && \
    [ "${doExternalDownload}" = "false" ]; then

    doCreate=true
    doCheck=true
fi


# sanity check on input for "testRun"
# convert input to integer or, if it is not an integer, to 0
int() { printf '%d' ${1:-} 2>/dev/null || :; }

if [ $(int "${testRun}") -eq 0 ] ; then
    echo "Provided option -t '""${testRun}""' is zero or not an integer."
    exit 1
fi


#------ CLEAN ------------------------------------------------------------------
if [ "${doCleanTestRuns}" = "true" ] || [ "${doCleanTests}" = "true" ]; then
    if [ "${testRun}" -gt 0 ]; then
        flag0="00"
        testRunTag="testRun-""${flag0:${#testRun}:${#flag0}}${testRun}"

        if [ "${doCleanTestRuns}" = "true" ]; then
            find "${dir_ncTestRuns}"/results/testRuns \
                -iname "${testRunTag}*" -type d \
                -exec rm -rf {} +
        fi

        if [ "${doCleanTests}" = "true" ]; then
            find "${dir_ncTestRuns}"/results/testRunsTemplates \
                -iname "${testRunTag}*" -type d \
                -exec rm -rf {} +
            find "${dir_ncTestRuns}"/results/testRuns \
                -iname "${testRunTag}*" -type d \
                -exec rm -rf {} +
        fi
    else
        if [ "${doCleanTestRuns}" = "true" ]; then
            rm -rf "${dir_ncTestRuns}"/results/testRuns > /dev/null 2>&1
        fi

        if [ "${doCleanTests}" = "true" ]; then
            rm -rf "${dir_ncTestRuns}"/results/domainTemplates > /dev/null 2>&1
            rm -rf "${dir_ncTestRuns}"/results/testRuns > /dev/null 2>&1
            rm -rf "${dir_ncTestRuns}"/results/testRunsTemplates > /dev/null 2>&1
        fi
    fi
fi

if [ "${doCleanReference}" = "true" ]; then
    rm -rf "${dir_ncTestRuns}"/results/referenceRun > /dev/null 2>&1
fi


#------ DOWNLOAD EXTERNAL DATA -------------------------------------------------
if [ "${doExternalDownload}" = "true" ]; then
    echo $'\n'"Download data from external datasets ..."

    Rscript \
        "${dir_ncTestRuns}"/scripts/Rscript__ncTestRuns_00_downloadExternalWeatherData.R \
        --path-to-inWeather="${dir_ncTestRuns}"/data-raw/inWeather
fi

if [ ! "${doCreate}" = "true" ] && [ ! "${doCheck}" = "true" ]; then
    exit 0
fi


#------ FUNCTIONALITY ----------------------------------------------------------
#--- Check that Rscript is available
if ! command -v Rscript > /dev/null 2>&1 ; then
    echo "Error: R is not installed."
    exit 1
fi


#--- Check that existing SOILWAT2 is nc-based
# $1 SOILWAT2 (including path)
hasNotSW2nc() {
    local res=""
    local sw2="$1"
    local status=1

    if [ -f "${sw2}" ]; then
        res=$(eval '"$sw2" -v' 2>&1)
        if [[ "$res" != *"Capabilities: netCDF-c, udunits2"* ]]; then
            status=0
        fi
    fi

    return $status
}


#------ MAIN -------------------------------------------------------------------
echo $'\n'\
==================================================$'\n'\
"ncTestRuns"$'\n'\
==================================================

if [ ! -f "${sw2}" ] || hasNotSW2nc "${sw2}" ; then
    echo $'\n'"Compile nc-based SOILWAT2 ..."
    res=$(make clean CPPFLAGS=-DSWNC all)
fi

if hasNotSW2nc "${sw2}" ; then
    echo "Error: SOILWAT2 lacks netCDF capabilities."
    exit 1
fi

if [ ! -d "${dirOutRef}" ] && [ "${dirOutRef}" = "${dirOutRefDefault}" ]; then
    echo $'\n'"Run SOILWAT2 to create default reference output ..."
    make clean_example

    Rscript \
        "${dir_ncTestRuns}"/scripts/Rscript__ncTestRuns_00_createReferenceRun.R \
        --path-to-ncTestRuns="${dir_ncTestRuns}" \
        --path-to-sw2="${sw2}" \
        --path-to-referenceOutput="${dirOutRef}"
    status=$?

    if [ $status -ne 0 ]; then
        echo $'\n'"Failed to create default reference run ..."
        exit 1
    fi
fi

if [ -d "${dirOutRef}" ]; then
    echo $'\n'"Values from '""${dirOutRef}""' will be used as reference."
else
    echo $'\n'"Failed to locate reference output ..."
    exit 1
fi


if [ "${doCreate}" = "true" ]; then
    echo $'\n'"Create ncTestRuns ..."

    make clean_example

    Rscript \
        "${dir_ncTestRuns}"/scripts/Rscript__ncTestRuns_01_createTestRuns.R \
        --path-to-ncTestRuns="${dir_ncTestRuns}" \
        --path-to-sw2="${sw2}" \
        --testRuns="${testRun}"
fi


if [ "${doCheck}" = "true" ]; then
    echo $'\n'"Execute and check ncTestRuns ..."
    Rscript \
        "${dir_ncTestRuns}"/scripts/Rscript__ncTestRuns_02_checkTestRuns.R \
        --path-to-ncTestRuns="${dir_ncTestRuns}" \
        --path-to-sw2="${sw2}" \
        --path-to-referenceOutput="${dirOutRef}" \
        --testRuns="${testRun}"
fi


echo $'\n'$'\n'\
--------------------------------------------------$'\n'\
"Complete!"$'\n'\
--------------------------------------------------$'\n'
