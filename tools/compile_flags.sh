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


# Feature test compilation
# Attempt to compile a minimal program to check if compiler understands a flag
# $1 language: "CC" or "CXX"
# $2 flag to check, e.g., "-fstrict-flex-arrays=2"
check_compiler_flag() {
    local lang="$1"
    local FLAG="$2"
    local compiler=""

    case $lang in
        CC|cc) compiler="${CC:-cc}" ;;
        CXX|cxx) compiler="${CXX:-c++}" ;;

        *) echo "$lang"" is not implemented."; exit 1 ;;
    esac

    # -Werror is needed for compilers that only warn
    # about an unrecognized flag rather than exiting with an error.
    if echo 'int main(){return 0;}' | "$compiler" "$FLAG" -Werror -x c - -o /dev/null >/dev/null 2>&1; then
        return 0
    else
        return 1
    fi
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

    # Note: some of the flags require at least gcc13 or clang-15
    # Note: use -ftrivial-auto-var-init=zero for production code (not pattern)

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
        -fno-omit-frame-pointer \
        -fno-common \
        "

    # Use -fstrict-flex-arrays=3 or 2 if available
    if check_compiler_flag "$lang" "-fstrict-flex-arrays=3"; then
        flags="$flags -fstrict-flex-arrays=3"
    elif check_compiler_flag "$lang" "-fstrict-flex-arrays=2"; then
        flags="$flags -fstrict-flex-arrays=2"
    fi

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
