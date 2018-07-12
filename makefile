#-----------------------------------------------------------------------------------
# commands         explanations
#-----------------------------------------------------------------------------------
# make bin         compile the binary executable using optimizations
# make bint        same as 'make bin' plus moves a copy of the binary to the
#                  'testing/' folder
# make bint_run    same as 'make bint' plus executes the binary in the testing/ folder
# make lib         create SOILWAT2 library
# make test        compile unit tests in 'test/ folder with googletest
# make testci      compile unit tests with less severe flags due to older compiler versions in 'test/ folder with googletest
# make test_run    run unit tests (in a previous step compiled with 'make test' or 'make testci')
# make bin_debug   compile the binary executable in debug mode
# make bind        same as 'make bin_debug' plus moves a copy of the binary to the
#                  'testing/' folder
# make bind_valgrind	same as 'make bind' plus runs valgrind on the debug binary in the testing/ folder
# make cov         same as 'make test' but with code coverage support
# make cov_run     run unit tests and gcov on each source file (in a previous step
#                  compiled with 'make cov')
# make cleaner     delete all of the o files, test files, libraries, and the binary exe
# make test_clean  delete test files and libraries
# make cov_clean   delete files associated with code coverage
#-----------------------------------------------------------------------------------

uname_m = $(shell uname -m)


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


#------ COMMANDS AND STANDARDS
# CC = gcc
# CXX = g++
# AR = ar
# RM = rm

use_c11 = -std=c11
use_gnu++11 = -std=gnu++11		# gnu++11 required for googletest on Windows/cygwin


#------ FLAGS
# Diagnostic warning/error messages
warning_flags = -Wall -Wextra
warning_flags_bin = $(warning_flags) -Wpedantic -Werror

# Instrumentation options for debugging and testing
instr_flags = -fstack-protector-all
instr_flags_severe = -D_FORTIFY_SOURCE=2 -Wno-macro-redefined \
	-fsanitize=undefined -fsanitize=address
	# -fstack-protector-strong (gcc >= v4.9)
	# (gcc >= 4.0) -D_FORTIFY_SOURCE: lightweight buffer overflow protection to some memory and string functions
	# (gcc >= 4.8; llvm >= 3.1) -fsanitize=address: replaces `mudflap` run time checker; https://github.com/google/sanitizers/wiki/AddressSanitizer

# Precompiler and compiler flags and options
sw_CPPFLAGS = $(CPPFLAGS)
sw_CFLAGS = $(CFLAGS)
sw_CXXFLAGS = $(CXXFLAGS) -Wno-error=deprecated		# TODO: clang++: "treating 'c' input as 'c++' when in C++ mode"

bin_flags = -O2
debug_flags = -g -O0 -DSWDEBUG
cov_flags = -coverage

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
sources = $(sw_sources) SW_Main_lib.c SW_VegEstab.c SW_Control.c generic.c \
					rands.c Times.c mymemory.c filefuncs.c SW_Files.c SW_Model.c \
					SW_Site.c SW_SoilWater.c SW_Markov.c SW_Weather.c SW_Sky.c \
					SW_Output.c SW_Output_get_functions.c\
					SW_VegProd.c SW_Flow_lib.c SW_Flow.c SW_Carbon.c
objects = $(sources:.c=.o)

# Unfortunately, we currently cannot include 'SW_Output.c' because
#  - cannot increment expression of enum type (e.g., OutKey, OutPeriod)
#  - assigning to 'OutKey' from incompatible type 'int'
# ==> instead, we use 'SW_Output_mock.c' which provides mock versions of the
# public functions (but will result in some compiler warnings)
sources_tests = SW_Main_lib.c SW_VegEstab.c SW_Control.c generic.c \
					rands.c Times.c mymemory.c filefuncs.c SW_Files.c SW_Model.c \
					SW_Site.c SW_SoilWater.c SW_Markov.c SW_Weather.c SW_Sky.c \
					SW_Output_mock.c \
					SW_VegProd.c SW_Flow_lib.c SW_Flow.c SW_Carbon.c
objects_tests = $(sources_tests:.c=.o)


bin_sources = SW_Main.c SW_Output_outtext.c # SOILWAT2-standalone
bin_objects = $(bin_sources:.c=.o)


gtest = gtest
lib_gtest = lib$(gtest).a
GTEST_DIR = googletest/googletest
GTEST_SRCS_ = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)
GTEST_HEADERS = $(GTEST_DIR)/include/gtest/*.h $(GTEST_DIR)/include/gtest/internal/*.h



#------ TARGETS
bin : $(target)

lib : $(lib_target)

$(lib_target) :
		$(CC) $(sw_CPPFLAGS) $(sw_CFLAGS) $(bin_flags) $(warning_flags_bin) \
		$(use_c11) -c $(sources)

		-@$(RM) -f $(lib_target)
		$(AR) -rcs $(lib_target) $(objects)
		-@$(RM) -f $(objects)


$(lib_target_test) :
		$(CXX) $(sw_CPPFLAGS) $(sw_CXXFLAGS) $(debug_flags) $(warning_flags) \
		$(instr_flags) $(use_gnu++11) -c $(sources_tests)

		-@$(RM) -f $(lib_target_test)
		$(AR) -rcs $(lib_target_test) $(objects_tests)
		-@$(RM) -f $(objects_tests)

$(lib_target_severe) :
		$(CXX) $(sw_CPPFLAGS) $(sw_CXXFLAGS) $(debug_flags) $(warning_flags) \
		$(instr_flags_severe) $(use_gnu++11) -c $(sources_tests)

		-@$(RM) -f $(lib_target_severe)
		$(AR) -rcs $(lib_target_severe) $(objects_tests)
		-@$(RM) -f $(objects_tests)

$(lib_target_cov) :
		$(CXX) $(sw_CPPFLAGS) $(sw_CXXFLAGS) $(debug_flags) $(warning_flags) \
		$(instr_flags) $(cov_flags) $(use_gnu++11) -c $(sources_tests)

		-@$(RM) -f $(lib_target_cov)
		$(AR) -rcs $(lib_target_cov) $(objects_tests)
		-@$(RM) -f $(objects_tests)


$(target) : $(lib_target)
		$(CC) $(sw_CPPFLAGS) $(sw_CFLAGS) $(bin_flags) $(warning_flags_bin) \
		$(use_c11) \
		-o $(target) $(bin_sources) $(target_LDLIBS) $(sw_LDFLAGS)

bin_debug :
		$(CC) $(sw_CPPFLAGS) $(sw_CFLAGS) $(debug_flags) $(warning_flags) \
		$(instr_flags_severe) $(use_c11) -c $(sources_tests)

		-@$(RM) -f $(lib_target_severe)
		$(AR) -rcs $(lib_target_severe) $(objects_tests)
		-@$(RM) -f $(objects_tests)

		$(CC) $(sw_CPPFLAGS) $(sw_CFLAGS) $(debug_flags) $(warning_flags) \
		$(instr_flags_severe) $(use_c11) \
		-o $(target) $(bin_sources) $(severe_LDLIBS) $(sw_LDFLAGS)

bin_debug_ci :
		$(CC) $(sw_CPPFLAGS) $(sw_CFLAGS) $(debug_flags) $(warning_flags) \
		$(instr_flags) $(use_c11) -c $(sources_tests)

		-@$(RM) -f $(lib_target_test)
		$(AR) -rcs $(lib_target_test) $(objects_tests)
		-@$(RM) -f $(objects_tests)

		$(CC) $(sw_CPPFLAGS) $(sw_CFLAGS) $(debug_flags) $(warning_flags) \
		$(instr_flags) $(use_c11) \
		-o $(target) $(bin_sources) $(test_LDLIBS) $(sw_LDFLAGS)


.PHONY : bint
bint : bin
		cp $(target) testing/$(target)

.PHONY : bint_run
bint_run : bint
		./testing/$(target) -d ./testing -f files.in

.PHONY : bind
bind : bin_debug
		cp $(target) testing/$(target)

.PHONY : bind_valgrind
bind_valgrind : bin_debug_ci
		cp $(target) testing/$(target)
		valgrind -v --track-origins=yes --leak-check=full ./testing/$(target) -d ./testing -f files.in


# GoogleTest:
# based on section 'Generic Build Instructions' at
# https://github.com/google/googletest/tree/master/googletest)
#   1) build googletest library
#   2) compile SOILWAT2 test source file
lib_test : $(lib_gtest)

$(lib_gtest) :
		@$(CXX) $(sw_CPPFLAGS) $(sw_CXXFLAGS) $(use_gnu++11) \
		-isystem ${GTEST_DIR}/include -I${GTEST_DIR} \
		-pthread -c ${GTEST_DIR}/src/gtest-all.cc

		@$(AR) -r $(lib_gtest) gtest-all.o

test : $(lib_gtest) $(lib_target_severe)
		$(CXX) $(sw_CPPFLAGS) $(sw_CXXFLAGS) $(debug_flags) $(warning_flags) \
		$(instr_flags_severe) $(use_gnu++11) \
		-isystem ${GTEST_DIR}/include -pthread \
		test/*.cc -o $(bin_test) $(gtest_LDLIBS) $(severe_LDLIBS) $(sw_LDFLAGS)

testci : $(lib_gtest) $(lib_target_test)
		$(CXX) $(sw_CPPFLAGS) $(sw_CXXFLAGS) $(debug_flags) $(warning_flags) \
		$(instr_flags) $(use_gnu++11) \
		-isystem ${GTEST_DIR}/include -pthread \
		test/*.cc -o $(bin_test) $(gtest_LDLIBS) $(test_LDLIBS) $(sw_LDFLAGS)

.PHONY : test_run
test_run :
		./$(bin_test)

cov : cov_clean $(lib_gtest) $(lib_target_cov)
		$(CXX) $(sw_CPPFLAGS) $(sw_CXXFLAGS) $(debug_flags) $(warning_flags) \
		$(instr_flags) $(cov_flags) $(use_gnu++11) \
		-isystem ${GTEST_DIR}/include -pthread \
		test/*.cc -o $(bin_test) $(gtest_LDLIBS) $(cov_LDLIBS) $(sw_LDFLAGS)

.PHONY : cov_run
cov_run : cov
		./$(bin_test)
		./run_gcov.sh




.PHONY : clean1
clean1 :
		-@$(RM) -f $(objects) $(bin_objects)

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
		-@$(RM) -f $(objects_tests)

.PHONY : cov_clean
cov_clean :
		-@$(RM) -f $(lib_target_cov) *.gcda *.gcno *.gcov
		-@$(RM) -fr *.dSYM

.PHONY : cleaner
cleaner : clean1 clean2 bint_clean test_clean cov_clean

.PHONY : clean
clean : cleaner
