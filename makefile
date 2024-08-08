#-------------------------------------------------------------------------------
# commands         explanations
#-------------------------------------------------------------------------------
# make help        display the top of this file, i.e., print explanations
#
# make compiler_version print version information of CC and CXX compilers
#
# --- Binary executable ------
# make             create the binary executable 'SOILWAT2'
# make all
#
# make bin_run     same as 'make bin' plus execute the binary for tests/example/
#                  (previously, `make bint_run`; target `bint` is obsolete)
#
# --- Documentation ------
# make doc         create html documentation for SOILWAT2 using doxygen
#
# make doc_open    open documentation
#
# --- SOILWAT2 library ------
# make lib         create SOILWAT2 library (utilized by SOILWAT2 and STEPWAT2)
# make libr        create SOILWAT2 library (utilized by rSOILWAT2)
#
# --- Tests ------
# make test        create a test binary executable that includes the unit tests
#                  in 'tests/gtests/' (using googletest)
# make test_run    execute the test binary
# make test_severe similar to `make test_run` with stricter flags for warnings
#                  and instrumentation; consider cleaning previous build
#                  artifacts beforehand, e.g., `make clean_test`
# make test_sanitizer  similar to `make test_severe` with stricter
#                  sanitizer settings; consider cleaning previous build
#                  artifacts beforehand, e.g., `make clean_test`
# make test_leaks  similar to `make test_run` but using `leaks` program;
#                  consider cleaning previous build artifacts beforehand,
#                  e.g., `make clean_test`
# make test_reprnd similar to `make test_run`, i.e., execute the test binary
#                  repeatedly while randomly shuffling tests
# make test_rep3rnd   similar to `make test_run`, i.e., execute the test binary
#                  three times while randomly shuffling tests
#
# make bin_run     runs the executable 'SOILWAT2' on the "example/" inputs
# make bin_debug   similar to `make bin_run` with debug settings;
#                  consider cleaning previous build artifacts beforehand,
#                  e.g., `make clean_build`
# make bin_debug_severe   similar to `make bin_debug` stricter flags for
#                  warnings and instrumentation; consider cleaning previous
#                  build artifacts beforehand, e.g., `make clean_build`
# make bin_sanitizer   similar to `make bin_debug_severe` with stricter
#                  sanitizer settings; consider cleaning previous build
#                  artifacts beforehand, e.g., `make clean_build`
# make bin_leaks   similar to `make bin_run` but using `leaks` program;
#                  consider cleaning previous build artifacts beforehand,
#                  e.g., `make clean_test`
#
# --- Code coverage ------
# make cov         same as 'make test_run' with code coverage support and
#                  running `gcov` on each source file
#                  (previously, `make cov cov_run`); consider cleaning
#                  previous artifacts beforehand, e.g., `make clean_cov`;
#                  use matching compiler and `gcov` versions, e.g.,
#                  for gcc `CXX=g++ GCOV=gcov make clean clean_cov cov`
#                  and for clang v14
#                  `CXX=clang++ GCOV="llvm-cov-mp-14 gcov" make clean clean_cov cov`
#
# --- Cleanup ------
# make clean       same as 'make clean_bin clean_build clean_test';
#                  does not clean artifacts from code coverage or documentation
# make clean_bin   delete 'bin/' (binary and test executables and library)
# make clean_build delete binary executable build artifacts
# make clean_test  delete test executable build artifacts
# make clean_cov   delete code coverage artifacts
# make clean_doc   delete documentation
#-------------------------------------------------------------------------------

#------ Requirement: GNU make
SHELL = /bin/sh
.SUFFIXES:
.SUFFIXES: .c .cc .o


#------ Identification
SW2_VERSION := "$(shell git describe --abbrev=7 --dirty --always --tags)"
HOSTNAME ?= "$(shell uname -sn)"
USERNAME ?= "$(USER)"

sw_info := -DSW2_VERSION=\"$(SW2_VERSION)\" \
					-DUSERNAME=\"$(USERNAME)\" \
					-DHOSTNAME=\"$(HOSTNAME)\"


#------ PATHS
# directory for source code (including headers) of SOILWAT2
dir_src := src
# directory for unit test source code
dir_test := tests/gtests
# directories that contain submodules
dir_pcg := external/pcg
dir_gtest := external/googletest/googletest
dir_gmock := external/googletest/googlemock

# output directory for executables, shared objects, libraries
dir_bin := bin
# directories for intermediate (ephemeral) compile objects
dir_build := build
dir_build_sw2 := $(dir_build)/sw2
dir_build_test := $(dir_build)/test



#------ OUTPUT NAMES
target := SOILWAT2
lib_sw2 := $(dir_bin)/lib$(target).a
lib_rsw2 := $(dir_bin)/libr$(target).a
bin_sw2 := $(dir_bin)/$(target)

target_test = $(target)_test
lib_test := $(dir_build_test)/lib$(target_test).a
bin_test := $(dir_bin)/sw_test

gtest := gtest
gmock := gmock
lib_gtest := $(dir_build_test)/lib$(gtest).a
lib_gmock := $(dir_build_test)/lib$(gmock).a



#------ COMMANDS
# CC = gcc or clang
# CXX = g++ or clang++
# AR = ar
# RM = rm


#------ netCDF SUPPORT
# `CPPFLAGS=-DSWNETCDF make all`
# `CPPFLAGS='-DSWNETCDF -DSWUDUNITS' make all`

# User-specified paths to netCDF header and library:
#   `CPPFLAGS=-DSWNETCDF NC_CFLAGS="-I/path/to/include" NC_LIBS="-L/path/to/lib" make all`

# User-specified paths to headers and libraries of netCDF, udunits2 and expat:
#   `CPPFLAGS='-DSWNETCDF -DSWUDUNITS' NC_CFLAGS="-I/path/to/include" UD_CFLAGS="-I/path/to/include" EX_CFLAGS="-I/path/to/include" NC_LIBS="-L/path/to/lib" UD_LIBS="-L/path/to/lib" EX_LIBS="-L/path/to/lib" make all`

ifneq (,$(findstring -DSWNETCDF,$(CPPFLAGS)))
  # define makefile variable SWNETCDF if defined via CPPFLAGS
  SWNETCDF = 1
endif

ifneq (,$(findstring -DSWUDUNITS,$(CPPFLAGS)))
  # define makefile variable SWUDUNITS if defined via CPPFLAGS
  SWUDUNITS = 1
endif

ifdef SWNETCDF
  ifndef NC_CFLAGS
    # use `nc-config` if user did not provide NC_CFLAGS
    sw_NC_CFLAGS := $(shell nc-config --cflags)
  else
    sw_NC_CFLAGS := $(NC_CFLAGS)
  endif

  ifndef NC_LIBS
    # use `nc-config` if user did not provide NC_LIBS
    sw_NC_LIBS := $(shell nc-config --libs)
  else
    sw_NC_LIBS := $(NC_LIBS)
  endif

  ifeq (,$(findstring -lnetcdf,$(sw_NC_LIBS)))
    # Add '-lnetcdf' if not already present
    sw_NC_LIBS += -lnetcdf
  endif

  ifdef SWUDUNITS
    ifndef UD_CFLAGS
      tmp := $(firstword $(sw_NC_CFLAGS))
      ifneq (,$(findstring -I,$(tmp)))
        # assume headers are at same include path as those for nc
        sw_UD_CFLAGS := $(tmp)/udunits2
      else
        sw_UD_CFLAGS :=
      endif
    else
      sw_UD_CFLAGS := $(UD_CFLAGS)
    endif

    ifndef UD_LIBS
      sw_UD_LIBS := -ludunits2
    else
      sw_UD_LIBS := $(UD_LIBS)
    endif

    ifeq (,$(findstring -ludunits2,$(sw_UD_LIBS)))
      # Add '-ludunits2' if not already present
      sw_UD_LIBS += -ludunits2
    endif

    ifndef EX_LIBS
      sw_EX_LIBS :=
    else
      sw_EX_LIBS := $(EX_LIBS)
      ifeq (,$(findstring -lexpat,$(sw_EX_LIBS)))
        # Add '-lexpat' if not already present
        sw_EX_LIBS += -lexpat
      endif
    endif

  else
    # if SWUDUNITS is not defined, then unset
    sw_UD_CFLAGS :=
    sw_EX_CFLAGS :=
    sw_UD_LIBS :=
    sw_EX_LIBS :=
  endif

else
  # if SWNETCDF is not defined, then unset
  SWUDUNITS :=
  sw_NC_CFLAGS :=
  sw_UD_CFLAGS :=
  sw_EX_CFLAGS :=
  sw_NC_LIBS :=
  sw_UD_LIBS :=
  sw_EX_LIBS :=
endif




#------ STANDARDS
# googletest requires c++17 and POSIX API
# see https://github.com/google/oss-policies-info/blob/main/foundational-cxx-support-matrix.md
#
# cygwin does not enable POSIX API by default
# --> enable by defining `_POSIX_C_SOURCE=200809L`
#     (or `-std=gnu++11` or `_GNU_SOURCE`)
# see https://github.com/google/googletest/issues/813 and
# see https://github.com/google/googletest/pull/2839#issue-613300962

set_std := -std=c99
set_std++_tests := -std=c++17


#------ FLAGS
# Diagnostic warning/error messages
warning_flags := -Wall -Wextra

# Don't use 'warning_flags_severe*' for production builds and rSOILWAT2
warning_flags_severe := \
	$(warning_flags) \
	-Wpedantic \
	-Werror \
	-Wcast-align \
	-Wmissing-declarations \
	-Wredundant-decls

warning_flags_severe_cc := \
	$(warning_flags_severe) \
	-Wstrict-prototypes # '-Wstrict-prototypes' is valid for C/ObjC but not for C++

warning_flags_severe_cxx := \
	$(warning_flags_severe) \
	-Wno-error=deprecated
	# TODO: address underlying problems so that we can eliminate
	# `-Wno-error=deprecated`
	# (https://github.com/DrylandEcology/SOILWAT2/issues/208):
	# "treating 'c' input as 'c++' when in C++ mode, this behavior is deprecated"


# Instrumentation options for debugging and testing
instr_flags := -fstack-protector-all

instr_flags_severe := \
	$(instr_flags) \
	-fsanitize=undefined \
	-fsanitize=address \
	-fno-omit-frame-pointer \
	-fno-common
	# -fstack-protector-strong (gcc >= v4.9)
	# (gcc >= 4.0) -D_FORTIFY_SOURCE: lightweight buffer overflow protection to some memory and string functions
	# (gcc >= 4.8; llvm >= 3.1) -fsanitize=address: AdressSanitizer: replaces `mudflap` run time checker; https://github.com/google/sanitizers/wiki/AddressSanitizer
	#   -fno-omit-frame-pointer: allows fast unwinder to work properly for ASan
	#   -fno-common: allows ASan to instrument global variables
	# (gcc >= 4.9; llvm >= 3.3) -fsanitize=undefined: UndefinedBehaviorSanitizer


# Precompiler and compiler flags and options
sw_CPPFLAGS := $(CPPFLAGS) $(sw_info) -MMD -MP -I.
sw_CPPFLAGS_bin := $(sw_CPPFLAGS) -I$(dir_build_sw2)
sw_CPPFLAGS_test := $(sw_CPPFLAGS) -I$(dir_build_test)
sw_CFLAGS := $(CFLAGS) $(sw_NC_CFLAGS) $(sw_UD_CFLAGS) $(sw_EX_CFLAGS)
sw_CXXFLAGS := $(CXXFLAGS) $(sw_NC_CFLAGS) $(sw_UD_CFLAGS) $(sw_EX_CFLAGS)

# `SW2_FLAGS` can be used to pass in additional flags
bin_flags := -O2 -fno-stack-protector $(SW2_FLAGS)
debug_flags := -g -O0 -DSWDEBUG $(SW2_FLAGS)
#cov_flags := -O0 -coverage
gtest_flags := -D_POSIX_C_SOURCE=200809L # googletest requires POSIX API


# Linker flags and libraries
# order of libraries is important for GNU gcc (libSOILWAT2 depends on libm)
sw_LDFLAGS_bin := $(LDFLAGS) -L$(dir_bin)
sw_LDFLAGS_test := $(LDFLAGS) -L$(dir_bin) -L$(dir_build_test)
sw_LDLIBS := $(LDLIBS) $(sw_NC_LIBS) $(sw_UD_LIBS) $(sw_EX_LIBS) -lm

target_LDLIBS := -l$(target) $(sw_LDLIBS)
test_LDLIBS := -l$(target_test) $(sw_LDLIBS)
gtest_LDLIBS := -l$(gtest)
gmock_LDLIBS := -l$(gmock)


#------ CODE FILES
# SOILWAT2 files
sources_core := \
	$(dir_src)/SW_Main_lib.c \
	$(dir_src)/SW_VegEstab.c \
	$(dir_src)/SW_Control.c \
	$(dir_src)/generic.c \
	$(dir_src)/rands.c \
	$(dir_src)/Times.c \
	$(dir_src)/mymemory.c \
	$(dir_src)/filefuncs.c \
	$(dir_src)/SW_Files.c \
	$(dir_src)/SW_Model.c \
	$(dir_src)/SW_Site.c \
	$(dir_src)/SW_SoilWater.c \
	$(dir_src)/SW_Markov.c \
	$(dir_src)/SW_Weather.c \
	$(dir_src)/SW_Sky.c \
	$(dir_src)/SW_VegProd.c \
	$(dir_src)/SW_Flow_lib_PET.c \
	$(dir_src)/SW_Flow_lib.c \
	$(dir_src)/SW_Flow.c \
	$(dir_src)/SW_Carbon.c \
	$(dir_src)/SW_Domain.c \
	$(dir_src)/SW_Output.c \
	$(dir_src)/SW_Output_get_functions.c \
	$(dir_src)/SW_Output_outarray.c \
	$(dir_src)/SW_Output_outtext.c

ifdef SWNETCDF
sources_core += $(dir_src)/SW_netCDF.c
endif

sources_lib = $(sources_core)
objects_lib = $(sources_lib:$(dir_src)/%.c=$(dir_build_sw2)/%.o)


# Code for SOILWAT2-standalone program
sources_bin := $(dir_src)/SW_Main.c
objects_bin := $(sources_bin:$(dir_src)/%.c=$(dir_build_sw2)/%.o)


# Code for SOILWAT2-test library (include all source code)
sources_lib_test := $(sources_core)
objects_lib_test := $(sources_lib_test:$(dir_src)/%.c=$(dir_build_test)/%.o)


# Unit test files
sources_test := $(wildcard $(dir_test)/*.cc)
objects_test := $(sources_test:$(dir_test)/%.cc=$(dir_build_test)/%.o)


# PCG random generator files (not used by rSOILWAT2)
sources_pcg := $(dir_pcg)/pcg_basic.c
objects_lib_pcg := $(sources_pcg:$(dir_pcg)/%.c=$(dir_build_sw2)/%.o)
objects_test_pcg := $(sources_pcg:$(dir_pcg)/%.c=$(dir_build_test)/%.o)


# Google test code
GTEST_SRCS_ := $(dir_gtest)/src/*.cc $(dir_gtest)/src/*.h $(GTEST_HEADERS)
GTEST_HEADERS := $(dir_gtest)/include/gtest/*.h $(dir_gtest)/include/gtest/internal/*.h

GMOCK_SRCS_ := $(dir_gmock)/src/*.cc $(GMOCK_HEADERS)
GMOCK_HEADERS := $(dir_gmock)/include/gmock/*.h $(dir_gmock)/include/gmock/internal/*.h $(GTEST_HEADERS)



#------ TARGETS
all : $(bin_sw2)

lib : $(lib_sw2)

libr : $(lib_rsw2)

test : $(bin_test)

.PHONY : all lib libr test



#--- SOILWAT2 library (utilized by SOILWAT2 and STEPWAT2)
$(lib_sw2) : $(objects_lib) $(objects_lib_pcg) | $(dir_bin)
		$(AR) -rcs $(lib_sw2) $(objects_lib) $(objects_lib_pcg)

#--- SOILWAT2 library without pcg (utilized by rSOILWAT2)
$(lib_rsw2) : $(objects_lib) | $(dir_bin)
		$(AR) -rcs $(lib_sw2) $(objects_lib)


#--- SOILWAT2 stand-alone executable (utilizing SOILWAT2 library)
$(bin_sw2) : $(lib_sw2) $(objects_bin) | $(dir_bin)
		$(CC) $(bin_flags) $(warning_flags) $(set_std) $(objects_bin) $(sw_LDFLAGS_bin) $(target_LDLIBS) -o $(bin_sw2)


#--- Unit test library and executable
$(lib_test) : $(objects_lib_test) $(objects_test_pcg) | $(dir_build_test)
		$(AR) -rcs $(lib_test) $(objects_lib_test) $(objects_test_pcg)

$(bin_test) : $(lib_gtest) $(lib_gmock) $(lib_test) $(objects_test) | $(dir_bin)
		$(CXX) $(gtest_flags) $(debug_flags) $(warning_flags) \
		$(instr_flags) $(set_std++_tests) \
		-isystem ${dir_gtest}/include \
                -isystem ${dir_gmock}/include -pthread \
		$(objects_test) $(sw_LDFLAGS_test) $(gtest_LDLIBS) $(gmock_LDLIBS) $(test_LDLIBS) -o $(bin_test)

# GoogleTest library
# based on section 'Generic Build Instructions' at
# https://github.com/google/googletest/tree/master/googletest)
#   1) build googletest library
#   2) compile SOILWAT2 test source file
$(lib_gtest) : | $(dir_build_test)
		$(CXX) $(sw_CPPFLAGS_test) $(sw_CXXFLAGS) $(gtest_flags) $(set_std++_tests) \
		-isystem ${dir_gtest}/include -I${dir_gtest} \
                -isystem ${dir_gmock}/include -I${dir_gmock} \
		-pthread -c ${dir_gtest}/src/gtest-all.cc -o $(dir_build_test)/gtest-all.o

		$(AR) -r $(lib_gtest) $(dir_build_test)/gtest-all.o

$(lib_gmock) : | $(dir_build_test)
		 $(CXX) $(sw_CPPFLAGS_test) $(sw_CXXFLAGS) $(gtest_flags) $(set_std++_tests) \
		 -isystem ${dir_gtest}/include -I${dir_gtest} \
                 -isystem ${dir_gmock}/include -I${dir_gmock} \
		 -pthread -c ${dir_gmock}/src/gmock-all.cc -o $(dir_build_test)/gmock-all.o

		 $(AR) -r $(lib_gmock) $(dir_build_test)/gmock-all.o


#--- Compile source files for library and executable
$(dir_build_sw2)/%.o: $(dir_src)/%.c | $(dir_build_sw2)
		$(CC) $(sw_CPPFLAGS_bin) $(sw_CFLAGS) $(bin_flags) $(warning_flags) $(set_std) -c $< -o $@

$(dir_build_sw2)/%.o: $(dir_pcg)/%.c | $(dir_build_sw2)
		$(CC) $(sw_CPPFLAGS_bin) $(sw_CFLAGS) $(bin_flags) $(warning_flags) $(set_std) -c $< -o $@


#--- Compile source files for unit tests
$(dir_build_test)/%.o: $(dir_src)/%.c | $(dir_build_test)
		$(CXX) $(sw_CPPFLAGS_test) $(sw_CXXFLAGS) $(gtest_flags) $(debug_flags) $(warning_flags) $(instr_flags) $(set_std++_tests) -c $< -o $@

$(dir_build_test)/%.o: $(dir_pcg)/%.c | $(dir_build_test)
		$(CXX) $(sw_CPPFLAGS_test) $(sw_CXXFLAGS) $(gtest_flags) $(debug_flags) $(warning_flags) $(instr_flags) $(set_std++_tests) -c $< -o $@

$(dir_build_test)/%.o: $(dir_test)/%.cc | $(dir_build_test)
		$(CXX) $(sw_CPPFLAGS_test) $(sw_CXXFLAGS) $(gtest_flags) $(debug_flags) $(warning_flags) $(instr_flags) $(set_std++_tests) \
                -isystem ${dir_gmock}/include \
                -isystem ${dir_gtest}/include -pthread -c $< -o $@


#--- Create directories
$(dir_bin) $(dir_build_sw2) $(dir_build_test):
		-@mkdir -p $@


#--- Include "makefiles" created by the CPPFLAG options `-MMD -MP`
#-include $(objects_bin:.o=.d) $(objects_lib:.o=.d) $(objects_lib_pcg:.o=.d)



#--- Convenience targets for testing
.PHONY : bin_run
bin_run : all
		$(bin_sw2) -d ./tests/example -f files.in

.PHONY : test_run
test_run : test
		$(bin_test)

.PHONY : test_severe
test_severe :
		./tools/run_test_severe.sh

.PHONY : test_sanitizer
test_sanitizer :
		./tools/run_test_sanitizer.sh

.PHONY : test_leaks
test_leaks : test
		./tools/run_test_leaks.sh

.PHONY : test_reprnd
test_reprnd : test
		$(bin_test) --gtest_shuffle --gtest_repeat=-1

.PHONY : test_rep3rnd
test_rep3rnd : test
		$(bin_test) --gtest_shuffle --gtest_repeat=3

.PHONY : bin_debug
bin_debug :
		./tools/run_debug.sh

.PHONY : bin_debug_severe
bin_debug_severe :
		./tools/run_debug_severe.sh

.PHONY : bin_sanitizer
bin_sanitizer :
		./tools/run_bin_sanitizer.sh

.PHONY : bin_leaks
bin_leaks : all
		./tools/run_bin_leaks.sh


#--- Convenience targets for code coverage
.PHONY : cov
cov :
		./tools/run_gcov.sh


#--- Targets for clang-tidy
tidy-bin: $(sources_lib) $(sources_bin)
	clang-tidy --config-file=.clang-tidy $(sources_lib) $(sources_bin) -- $(sw_CPPFLAGS_bin) $(sw_CFLAGS) $(bin_flags) $(warning_flags) $(set_std)

tidy-test: $(sources_test)
	clang-tidy --config-file=.clang-tidy_swtest $(sources_test) -- $(sw_CPPFLAGS_test) $(sw_CXXFLAGS) $(gtest_flags) $(debug_flags) $(warning_flags) $(instr_flags) $(set_std++_tests) -isystem ${dir_gmock}/include -isystem ${dir_gtest}/include


#--- Convenience targets for documentation
.PHONY : doc
doc :
		./tools/run_doxygen.sh

.PHONY : doc_open
doc_open :
		./tools/doc_open.sh


#--- Clean up
.PHONY : clean
clean: clean_bin clean_build clean_test
		-@$(RM) -r $(dir_build)

.PHONY : clean_bin
clean_bin:
		-@$(RM) -r $(dir_bin)

.PHONY : clean_build
clean_build:
		-@$(RM) -r $(dir_build_sw2)
		-@$(RM) -f $(bin_sw2) $(lib_sw2) $(lib_rsw2)

.PHONY : clean_test
clean_test:
		-@$(RM) -r $(dir_build_test)
		-@$(RM) -f $(bin_test)

.PHONY : clean_cov
clean_cov : clean_test
		-@$(RM) -f *.gcov
		-@$(RM) -fr $(dir_bin)/*.dSYM

.PHONY : clean_doc
clean_doc :
		-@$(RM) -fr doc/html/
		-@$(RM) -f doc/log_doxygen.log


#--- Target for makefile help
.PHONY : help
help :
		less makefile


#--- Target to print compiler information
.PHONY : compiler_version
compiler_version :
		@(echo CC version:)
		@(echo `"$(CC)" --version`)
		@(echo CXX version:)
		@(echo `"$(CXX)" --version`)
		@(echo make version:)
		@(echo `"$(MAKE)" --version`)
