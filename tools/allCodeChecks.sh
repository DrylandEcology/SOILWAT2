#!/bin/bash

#------ . ------
# All SOILWAT2 code checks
#
# Run this script with `tools/run_allCode.sh`
#
#------ . ------


echo $'\n'\
==================================================$'\n'\
"All code checks ..."$'\n'\
==================================================

echo $'\n'\
==================================================$'\n'\
"Check documentation ..."$'\n'\
--------------------------------------------------
if command -v doxygen > /dev/null 2>&1 ; then
    make doc
else
    echo "Skip checks with doxygen."
fi


echo $'\n'\
==================================================$'\n'\
"Format code ..."$'\n'\
--------------------------------------------------
if command -v clang-format > /dev/null 2>&1 ; then
    tools/run_format.sh
else
    echo "Skip checks with clang-format."
fi


echo $'\n'\
==================================================$'\n'\
"Tidy code ..."$'\n'\
--------------------------------------------------
if command -v clang-tidy > /dev/null 2>&1 ; then
    tools/run_tidy.sh
else
    echo "Skip checks with clang-tidy."
fi


echo $'\n'\
==================================================$'\n'\
"Check included headers ..."$'\n'\
--------------------------------------------------
if command -v include-what-you-use > /dev/null 2>&1 ; then
    tools/run_iwyu.sh
else
    echo "Skip checks with include-what-you-use."
fi


echo $'\n'\
==================================================$'\n'\
"Completed all code checks!"$'\n'\
==================================================
