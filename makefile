#-----------------------------------------------------------------------------------
# commands         explanations
#-----------------------------------------------------------------------------------
# make bin         compile the binary executable using optimizations
# make bint        same as 'make bin' plus moves a copy of the binary to the
#                  'testing/' folder
# make bint_run    same as 'make bint' plus executes the binary in the testing/ folder
# make lib         create SOILWAT2 library
# make test        compile unit tests in 'test/ folder with googletest
# make test_run    run unit tests (in a previous step compiled with 'make test')
# make cov         same as 'make test' but with code coverage support
# make cov_run     run unit tests and gcov on each source file (in a previous step
#                  compiled with 'make cov')
# make cleaner     delete all of the o files, test files, libraries, and the binary exe
# make test_clean  delete test files and libraries
# make cov_clean   delete files associated with code coverage
#-----------------------------------------------------------------------------------

uname_m = $(shell uname -m)

# CC = gcc
# CXX = g++
# AR = ar
CFLAGS = -O3 -Wall -Wextra -pedantic -std=c11
CXXFLAGS = -Wall -Wextra -std=gnu++11		# gnu++11 required for googletest on Windows/cygwin
CovFlags = -coverage -g -O0
LDFLAGS = -L.
LDLIBS = -l$(target) -lm						# order of libraries is important for GNU gcc (libSOILWAT2 depends on libm)

sources = SW_Main_lib.c SW_VegEstab.c SW_Control.c generic.c \
					rands.c Times.c mymemory.c filefuncs.c \
					SW_Files.c SW_Model.c SW_Site.c SW_SoilWater.c \
					SW_Markov.c SW_Weather.c SW_Sky.c SW_Output.c \
					SW_VegProd.c SW_Flow_lib.c SW_Flow.c SW_Carbon.c
objects = $(sources:.c=.o)

# Unfortunately, we cannot include 'SW_Output.c' currently because
#  - cannot increment expression of enum type (e.g., OutKey, OutPeriod)
#  - assigning to 'OutKey' from incompatible type 'int'
# ==> instead, we use 'SW_Output_mock.c' which provides mock versions of the public functions (but will result in some compiler warnings)
sources_tests = SW_Main_lib.c SW_VegEstab.c SW_Control.c generic.c \
					rands.c Times.c mymemory.c filefuncs.c \
					SW_Files.c SW_Model.c SW_Site.c SW_SoilWater.c \
					SW_Markov.c SW_Weather.c SW_Sky.c SW_Output_mock.c\
					SW_VegProd.c SW_Flow_lib.c SW_Flow.c SW_Carbon.c
objects_tests = $(sources_tests:.c=.o)


bin_sources = SW_Main.c
bin_objects = $(bin_sources:.c=.o)


target = SOILWAT2
bin_test = sw_test
lib_target = lib$(target).a
lib_target++ = lib$(target)++.a
lib_covtarget++ = libcov$(target)++.a

gtest = gtest
lib_gtest = lib$(gtest).a
GTEST_DIR = googletest/googletest
GTEST_SRCS_ = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)
GTEST_HEADERS = $(GTEST_DIR)/include/gtest/*.h \
                $(GTEST_DIR)/include/gtest/internal/*.h
gtest_CPPFLAGS = $(CPPFLAGS) -DSWDEBUG
gtest_LDLIBS = -l$(gtest) -l$(target)++ -lm
cov_LDLIBS = -l$(gtest) -lcov$(target)++ -lm

lib : $(lib_target)

$(lib_target) :
		$(CC) $(CPPFLAGS) $(CFLAGS) -c $(sources)
		@rm -f $(lib_target)
		$(AR) -rcs $(lib_target) $(objects)
		@rm -f $(objects)

$(lib_target++) :
		$(CXX) $(gtest_CPPFLAGS) $(CXXFLAGS) -c $(sources_tests)
		$(AR) -rcsu $(lib_target++) $(objects_tests)
		@rm -f $(objects_tests)

$(lib_covtarget++) :
		$(CXX) $(gtest_CPPFLAGS) $(CXXFLAGS) $(CovFlags) -c $(sources_tests)
		$(AR) -rcsu $(lib_covtarget++) $(objects_tests)
		@rm -f $(objects_tests)

bin : $(target)

$(target) : $(lib_target)
		$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $(target) $(bin_sources) $(LDLIBS)

.PHONY : bint
bint : bin
		cp $(target) testing/$(target)

.PHONY : bint_run
bint_run : bint
		./testing/$(target) -d ./testing -f files.in


# GoogleTest:
# based on section 'Generic Build Instructions' in https://github.com/google/googletest/tree/master/googletest)
#   1) build googletest library
#   2) compile SOILWAT2 test source file
lib_test : $(lib_gtest)

$(lib_gtest) :
		$(CXX) $(CPPFLAGS) $(CXXFLAGS) -isystem ${GTEST_DIR}/include -I${GTEST_DIR} \
			-pthread -c ${GTEST_DIR}/src/gtest-all.cc
		$(AR) -r $(lib_gtest) gtest-all.o

test : $(lib_gtest) $(lib_target++)
		$(CXX) $(gtest_CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -isystem ${GTEST_DIR}/include -pthread \
				test/*.cc -o $(bin_test) $(gtest_LDLIBS)

.PHONY : test_run
test_run :
		./$(bin_test)

cov : cov_clean $(lib_gtest) $(lib_covtarget++)
		$(CXX) $(gtest_CPPFLAGS) $(CXXFLAGS) $(CovFlags) $(LDFLAGS) -isystem ${GTEST_DIR}/include \
			-pthread test/*.cc -o $(bin_test) $(cov_LDLIBS)

.PHONY : cov_run
cov_run : cov
		./$(bin_test)
		./run_gcov.sh




.PHONY : clean1
clean1 :
		@rm -f $(objects) $(bin_objects)

.PHONY : clean2
clean2 :
		@rm -f $(target) $(lib_target) $(lib_target++)
		@rm -f testing/$(target)

.PHONY : bint_clean
bint_clean :
		@rm -f testing/Output/*

.PHONY : test_clean
test_clean :
		@rm -f gtest-all.o $(lib_gtest) $(bin_test)

.PHONY : cov_clean
cov_clean :
		@rm -f $(lib_covtarget++) *.gcda *.gcno *.gcov
		@rm -fr *.dSYM

.PHONY : cleaner
cleaner : clean1 clean2 bint_clean test_clean cov_clean
