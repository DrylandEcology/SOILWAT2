#!/bin/bash

# $1 optimization level: O0, O1
debug_flags() {
    local opt="$1"
    local res=""

    case $opt in
        O0) res="-g -O0 -DSWDEBUG" ;;
        O1) res="-g -O1 -DSWDEBUG" ;;

        *) res="-g -$opt -DSWDEBUG" ;;
    esac

    echo "$res"
}


# $1 language: "CC" or "CXX"
# $2 sanitizer: "", "sanitizer-no" or "sanitizer-yes"
#   optimization level of 1 is recommended for -fsanitize=undefined
compile_flags() {
    local res=""
    local lang="$1"
    local useSanitizer="$2"

    #--- List flags
    # see also recommendations by
    # https://best.openssf.org/Compiler-Hardening-Guides/Compiler-Options-Hardening-Guide-for-C-and-C++.html

    # Note: use -ftrivial-auto-var-init=zero for production code (not pattern)

    # TODO: set -fstrict-flex-arrays=3 (once we no longer use Apple Clang 15)
    # TODO: address underlying problems so that we can add -Wformat=2
    # TODO: address underlying problems so that we can add -Wconversion
    local flags="\
        -Wall \
        -Wextra \
        -Wpedantic \
        -Werror \
        -Wcast-align \
        -Wdeprecated \
        -Wformat \
        -Wimplicit-fallthrough \
        -Wmissing-declarations \
        -Wnull-dereference \
        -Wredundant-decls \
        -Wshadow \
        -U_FORTIFY_SOURCE \
        -D_FORTIFY_SOURCE=3 \
        -D_GLIBCXX_ASSERTIONS \
        -fno-delete-null-pointer-checks \
        -fno-strict-overflow \
        -fno-strict-aliasing \
        -ftrivial-auto-var-init=pattern \
        -fstack-protector-all \
        -fstack-protector-strong \
        -fstrict-aliasing \
        -fstrict-flex-arrays=2 \
        -fno-omit-frame-pointer \
        -fno-common \
        "

    # Flags valid for C but not C++
    local flags_cc="\
        -Werror=implicit \
        -Werror=incompatible-pointer-types \
        -Werror=int-conversion \
        -Wstrict-prototypes \
        "

    # TODO: address underlying problems so that we can eliminate
    # `-Wno-error=deprecated`
    # (https://github.com/DrylandEcology/SOILWAT2/issues/208):
    # treating 'c' input as 'c++' when in C++ mode, this behavior is deprecated
    local flags_cxx="-Wno-error=deprecated"

    local flags_sanitizer="\
        -fsanitize=undefined \
        -fsanitize=address \
        "


    #--- Put flags together for requested compilation
    res="$flags"

    case $lang in
        CC|cc) res="$res $flags_cc" ;;
        CXX|cxx) res="$res $flags_cxx" ;;

        *) echo "$lang"" is not implemented."; exit 1 ;;
    esac

    if test -z "$useSanitizer"; then
        useSanitizer="sanitizer-no"
    fi

    case $useSanitizer in
        sanitizer-no) ;;
        sanitizer-yes) res="$res $flags_sanitizer" ;;

        *) echo "$useSanitizer"" is not implemented."; exit 1 ;;
    esac

    echo $res
}
