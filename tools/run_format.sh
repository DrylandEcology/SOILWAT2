#!/bin/bash

# run clang-format v18 using .clang-format on all source and header files in
# `src/`, `include/` and `tests/gtests` (but not `external/`)

cfv=$(clang-format --version)

if [[ "${cfv}" == *"version 18."* ]]; then

    find src \( -iname '*.h' -o -iname '*.c' \) -print0 | xargs -0 clang-format -i --style=file
    find include \( -iname '*.h' \) -print0 | xargs -0 clang-format -i -style=file
    find tests/gtests \( -iname '*.h' -o -iname '*.cc' \) -print0 | xargs -0 clang-format -i --style=file

else
    echo "We need clang-format version 18 but found" "${cfv}"
    exit 1
fi
