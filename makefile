#-------------------------------------------------------------------------------
# commands         explanations
#-------------------------------------------------------------------------------
# make help        display the top of this file, i.e., print explanations
#
# make all         (synonym to 'bin'): compile the binary executable using
#                  optimizations
# make bint        same as 'make bin' plus move a copy of the binary to the
#                  'testing/' folder
# make bint_run    same as 'make bint' plus execute the binary in testing/
#
# make doc         create html documentation for SOILWAT2 using doxygen
#
# make doc_open    open documentation
#
# make lib         create SOILWAT2 library
#
# make test        compile unit tests in 'test/ folder (with googletest)
# make test_severe compile unit tests with severe flags in 'test/'
# make test_run    run unit tests (in a previous step compiled with 'make test'
#                  or 'make test_severe')
#
# make bin_debug   compile the binary executable in debug mode
# make bin_debug_severe   same as 'make bin_debug' but with severe flags
# make bind_valgrind      same as 'make bind' plus run valgrind on the debug
#                  binary in the testing/ folder
#
# make cov         same as 'make test' but with code coverage support
# make cov_run     run unit tests and gcov on each source file (in a previous
#                  step compiled with 'make cov')
#
# make clean       (synonym to 'cleaner'): delete all of the o files, test
#                  files, libraries, and the binary exe(s)
# make test_clean  delete test files and libraries
# make cov_clean   delete files associated with code coverage
# make doc_clean   delete documentation
#
# make compiler_version print version information of CC and CXX compilers
#-------------------------------------------------------------------------------

#------ Identification
SW2_VERSION := "$(shell git describe --abbrev=7 --dirty --always --tags)"
HOSTNAME := "$(shell uname -sn)"

sw_info = -DSW2_VERSION=\"$(SW2_VERSION)\" \
					-DUSERNAME=\"$(USER)\" \
					-DHOSTNAME=\"$(HOSTNAME)\"


#------ OUTPUT NAMES
target = SOILWAT2
bin_test = sw_test
target_test = $(target)_test
target_severe = $(target)_severe
target_cov = $(target)_cov

lib_target = lib$(target).a
lib_target_test = lib$(target_test).a
lib_target_severe = lib$(target_severe).a
lib_target_cov = lib$(target_cov).a


#------ COMMANDS
# CC = gcc
# CXX = g++
# AR = ar
# RM = rm


#------ STANDARDS
# googletest requires c++11 and POSIX API
# cygwin does not enable POSIX API by default (e.g., `strdup()` is missing)
# --> enable by defining `_POSIX_C_SOURCE=200809L`
#     (or `-std=gnu++11` or `_GNU_SOURCE`)
# see https://github.com/google/googletest/issues/813 and
# see https://github.com/google/googletest/pull/2839#issue-613300962

set_std = -std=c11
set_std_tests = -std=c11
set_std++_tests = -std=c++11


#------ FLAGS
# Diagnostic warning/error messages
warning_flags = -Wall -Wextra

# Don't use 'warning_flags_severe*' for production builds and rSOILWAT2
warning_flags_severe = \
	$(warning_flags) \
	-Wpedantic \
	-Werror \
	-Wcast-align \
	-Wmissing-declarations \
	-Wredundant-decls

warning_flags_severe_cc = \
	$(warning_flags_severe) \
	-Wstrict-prototypes # '-Wstrict-prototypes' is valid for C/ObjC but not for C++

warning_flags_severe_cxx = \
	$(warning_flags_severe) \
	-Wno-error=deprecated
	# TODO: address underlying problems so that we can eliminate
	# `-Wno-error=deprecated`
	# (https://github.com/DrylandEcology/SOILWAT2/issues/208):
	# "treating 'c' input as 'c++' when in C++ mode, this behavior is deprecated"


# Instrumentation options for debugging and testing
instr_flags = -fstack-protector-all

instr_flags_severe = \
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
sw_CPPFLAGS = $(CPPFLAGS) $(sw_info)
sw_CFLAGS = $(CFLAGS)
sw_CXXFLAGS = $(CXXFLAGS)

bin_flags = -O2 -fno-stack-protector
debug_flags = -g -O0 -DSWDEBUG
cov_flags = -O0 -coverage
gtest_flags = -D_POSIX_C_SOURCE=200809L # googletest requires POSIX API


# Linker flags and libraries
# order of libraries is important for GNU gcc (libSOILWAT2 depends on libm)
sw_LDFLAGS = $(LDFLAGS) -L.
sw_LDLIBS = $(LDLIBS) -lm

target_LDLIBS = -l$(target) $(sw_LDLIBS)
test_LDLIBS = -l$(target_test) $(sw_LDLIBS)
severe_LDLIBS = -l$(target_severe) $(sw_LDLIBS)
cov_LDLIBS = -l$(target_cov) $(sw_LDLIBS)

gtest_LDLIBS = -l$(gtest)


#------ CODE FILES
# SOILWAT2 files
sources_core = SW_Main_lib.c SW_VegEstab.c SW_Control.c generic.c \
					rands.c Times.c mymemory.c filefuncs.c SW_Files.c SW_Model.c \
					SW_Site.c SW_SoilWater.c SW_Markov.c SW_Weather.c SW_Sky.c \
					SW_VegProd.c SW_Flow_lib_PET.c SW_Flow_lib.c SW_Flow.c SW_Carbon.c

sources_lib = $(sw_sources) $(sources_core) SW_Output.c SW_Output_get_functions.c
objects_lib = $(sources_lib:.c=.o)


# Unfortunately, we currently cannot include 'SW_Output.c' because
#  - cannot increment expression of enum type (e.g., OutKey, OutPeriod)
#  - assigning to 'OutKey' from incompatible type 'int'
# ==> instead, we use 'SW_Output_mock.c' which provides mock versions of the
# public functions (but will result in some compiler warnings)
sources_lib_test = $(sources_core) SW_Output_mock.c
objects_lib_test = $(sources_lib_test:.c=.o)


sources_bin = SW_Main.c SW_Output_outtext.c # SOILWAT2-standalone
objects_bin = $(sources_bin:.c=.o)


# PCG random generator files
PCG_DIR = pcg
sources_pcg = $(PCG_DIR)/pcg_basic.c
objects_pcg = pcg_basic.o


# Google unit test files
gtest = gtest
lib_gtest = lib$(gtest).a
GTEST_DIR = googletest/googletest
GTEST_SRCS_ = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)
GTEST_HEADERS = $(GTEST_DIR)/include/gtest/*.h $(GTEST_DIR)/include/gtest/internal/*.h


#------ TARGETS
bin : $(target)

lib : $(lib_target)

$(lib_target) :
		$(CC) $(sw_CPPFLAGS) $(sw_CFLAGS) $(bin_flags) $(warning_flags) \
		$(set_std) -c $(sources_lib) $(sources_pcg)

		-@$(RM) -f $(lib_target)
		$(AR) -rcs $(lib_target) $(objects_lib) $(objects_pcg)
		-@$(RM) -f $(objects_lib) $(objects_pcg)


$(lib_target_test) :
		$(CC) $(sw_CPPFLAGS) $(sw_CFLAGS) $(debug_flags) $(warning_flags) \
		$(instr_flags) $(set_std_tests) -c $(sources_lib_test) $(sources_pcg)

		-@$(RM) -f $(lib_target_test)
		$(AR) -rcs $(lib_target_test) $(objects_lib_test) $(objects_pcg)
		-@$(RM) -f $(objects_lib_test) $(objects_pcg)

$(lib_target_severe) :	# needs CXX because of '*_severe' flags which must match test binary build
		$(CXX) $(sw_CPPFLAGS) $(sw_CXXFLAGS) $(debug_flags) $(warning_flags_severe_cxx) \
		$(instr_flags_severe) $(set_std++_tests) -c $(sources_lib_test) $(sources_pcg)

		-@$(RM) -f $(lib_target_severe)
		$(AR) -rcs $(lib_target_severe) $(objects_lib_test) $(objects_pcg)
		-@$(RM) -f $(objects_lib_test) $(objects_pcg)

$(lib_target_cov) :	# needs CXX because of 'coverage' flags which must match test binary build
		$(CXX) $(sw_CPPFLAGS) $(sw_CXXFLAGS) $(debug_flags) $(warning_flags) \
		$(instr_flags) $(cov_flags) $(set_std++_tests) -c $(sources_lib_test) $(sources_pcg)

		-@$(RM) -f $(lib_target_cov)
		$(AR) -rcs $(lib_target_cov) $(objects_lib_test) $(objects_pcg)
		-@$(RM) -f $(objects_lib_test) $(objects_pcg)


$(target) : $(lib_target)
		$(CC) $(sw_CPPFLAGS) $(sw_CFLAGS) $(bin_flags) $(warning_flags) \
		$(set_std) \
		-o $(target) $(sources_bin) $(target_LDLIBS) $(sw_LDFLAGS)

bin_debug_severe :
		$(CC) $(sw_CPPFLAGS) $(sw_CFLAGS) $(debug_flags) $(warning_flags_severe_cc) \
		$(instr_flags_severe) $(set_std) -c $(sources_lib_test) $(sources_pcg)

		-@$(RM) -f $(lib_target_severe)
		$(AR) -rcs $(lib_target_severe) $(objects_lib_test) $(objects_pcg)
		-@$(RM) -f $(objects_lib_test) $(objects_pcg)

		$(CC) $(sw_CPPFLAGS) $(sw_CFLAGS) $(debug_flags) $(warning_flags_severe_cc) \
		$(instr_flags_severe) $(set_std) \
		-o $(target) $(sources_bin) $(severe_LDLIBS) $(sw_LDFLAGS)

bin_debug : $(lib_target_test)
		$(CC) $(sw_CPPFLAGS) $(sw_CFLAGS) $(debug_flags) $(warning_flags) \
		$(instr_flags) $(set_std) \
		-o $(target) $(sources_bin) $(test_LDLIBS) $(sw_LDFLAGS)


.PHONY : bint
bint :
		cp $(target) testing/$(target)

.PHONY : bint_run
bint_run : bint
		./testing/$(target) -d ./testing -f files.in

.PHONY : bind_valgrind
bind_valgrind : bin_debug bint
		valgrind -v --track-origins=yes --leak-check=full ./testing/$(target) -d ./testing -f files.in


# GoogleTest:
# based on section 'Generic Build Instructions' at
# https://github.com/google/googletest/tree/master/googletest)
#   1) build googletest library
#   2) compile SOILWAT2 test source file
lib_test : $(lib_gtest)

$(lib_gtest) :
		$(CXX) $(sw_CPPFLAGS) $(sw_CXXFLAGS) $(gtest_flags) $(set_std++_tests) \
		-isystem ${GTEST_DIR}/include -I${GTEST_DIR} \
		-pthread -c ${GTEST_DIR}/src/gtest-all.cc

		$(AR) -r $(lib_gtest) gtest-all.o

test_severe : $(lib_gtest) $(lib_target_severe)
		$(CXX) $(sw_CPPFLAGS) $(sw_CXXFLAGS) $(gtest_flags) $(debug_flags) $(warning_flags_severe_cxx) \
		$(instr_flags_severe) $(set_std++_tests) \
		-isystem ${GTEST_DIR}/include -pthread \
		test/*.cc -o $(bin_test) $(gtest_LDLIBS) $(severe_LDLIBS) $(sw_LDFLAGS)

test : $(lib_gtest) $(lib_target_test)
		$(CXX) $(sw_CPPFLAGS) $(sw_CXXFLAGS) $(gtest_flags) $(debug_flags) $(warning_flags) \
		$(instr_flags) $(set_std++_tests) \
		-isystem ${GTEST_DIR}/include -pthread \
		test/*.cc -o $(bin_test) $(gtest_LDLIBS) $(test_LDLIBS) $(sw_LDFLAGS)

.PHONY : test_run
test_run :
		./$(bin_test)

cov : cov_clean $(lib_gtest) $(lib_target_cov)
		$(CXX) $(sw_CPPFLAGS) $(sw_CXXFLAGS) $(gtest_flags) $(debug_flags) $(warning_flags) \
		$(instr_flags) $(cov_flags) $(set_std++_tests) \
		-isystem ${GTEST_DIR}/include -pthread \
		test/*.cc -o $(bin_test) $(gtest_LDLIBS) $(cov_LDLIBS) $(sw_LDFLAGS)

.PHONY : cov_run
cov_run : cov
		./$(bin_test)
		./tools/run_gcov.sh


.PHONY : doc
doc :
		./tools/run_doxygen.sh


.PHONY : doc_open
doc_open :
		./tools/doc_open.sh


.PHONY : clean1
clean1 :
		-@$(RM) -f $(objects_lib) $(objects_bin) $(objects_pcg)

.PHONY : clean2
clean2 :
		-@$(RM) -f $(target) $(lib_target)
		-@$(RM) -f testing/$(target)

.PHONY : bint_clean
bint_clean :
		-@$(RM) -f testing/Output/*

.PHONY : test_clean
test_clean :
		-@$(RM) -f gtest-all.o $(lib_gtest) $(bin_test)
		-@$(RM) -f $(lib_target_test) $(lib_target_severe) $(lib_target_cov)
		-@$(RM) -fr *.dSYM
		-@$(RM) -f $(objects_lib_test)

.PHONY : cov_clean
cov_clean :
		-@$(RM) -f $(lib_target_cov) *.gcda *.gcno *.gcov
		-@$(RM) -fr *.dSYM

.PHONY : cleaner
cleaner : clean1 clean2 bint_clean test_clean cov_clean

.PHONY : clean
clean : cleaner

.PHONY : doc_clean
doc_clean :
		-@$(RM) -fr doc/html/
		-@$(RM) -f doc/log_doxygen.log


.PHONY : help
help :
		less makefile


.PHONY : compiler_version
compiler_version :
		@(echo CC version:)
		@(echo `"$(CC)" --version`)
		@(echo CXX version:)
		@(echo `"$(CXX)" --version`)
