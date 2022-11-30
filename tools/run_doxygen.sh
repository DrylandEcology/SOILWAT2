#!/bin/bash

# Create documentation using doxygen and report any errors/warnings except
# those warnings/errors listed as exceptions (doc/doxygen_exceptions.txt)


# Don't use set if run locally, e.g., with `make doc`
if [ ${CI} ]; then
  set -ev
  # The -e flag causes the script to exit as soon as one command returns a non-zero exit code
  # The -v flag makes the shell print all lines in the script before executing them
fi


doc_path="doc"                            # path to documentation
doxy=${doc_path}"/Doxyfile"               # Doxyfile
log=${doc_path}"/log_doxygen.log"         # logfile for doxygen output
log_tmp=${doc_path}"/log_doxygen_tmp.log"
doxexcept=${doc_path}"/doxygen_exceptions.txt" # filename that lists exceptions (one per line)

# Downgrade/upgrade Doxyfile in case this runs an different doxygen version, e.g.,
# travis-ci is currently on 1.8.11
#if [[ $(doxygen --version) != "1.8.17" ]]; then
#  echo "Upgrade Doxyfile to doxygen v"`doxygen --version`
  doxygen -u ${doxy} &>/dev/null
#fi

# Run doxygen and pipe all output to a log file
doxygen ${doxy} > ${log} 2>&1

# Eliminate warnings/errors that are known exceptions
# Make sure that there is such a (readable) file
if [ -r ${doxexcept} ]; then
  # Make sure that there are exceptions in the file
  if [ $(wc -w < ${doxexcept}) -ne 0 ]; then
    # Remove exceptions from the logfile
    grep --invert-match -f ${doxexcept} ${log} > ${log_tmp}
    mv ${log_tmp} ${log}
    rm -f ${log_tmp}
  fi
fi

# Fail if log file is completely empty (likely a grep-parsing error)
if [ $(wc -w < ${log}) -eq 0 ]; then
  echo "doxygen log file is empty pointing to a likely error."
  exit 1
fi

# Examine log file for remaining warnings/errors
warnings="$(grep -iE "warning|error" ${log} 2>&1 || echo "")"

if [ -n "${warnings}" ]; then
  echo "${warnings}"
  exit 1
fi
