#!/bin/bash

doc_file="doc/html/index.html"

if ! open "${doc_file}"; then
  if ! xdg-open "${doc_file}"; then
    echo "Failed to open documentation"
  fi
fi
