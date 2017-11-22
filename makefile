#-----------------------------------------------------------------------------------
# 05/24/2012 (DLM)
# 07/14/2017 (drs)
# for use in terminal while in the project source directory to make compiling easier
#-----------------------------------------------------------------------------------
# commands          explanations
#-----------------------------------------------------------------------------------
# make all         creates a shared object for use in rSOILWAT2 ('all' target is required
#                  by R CMD)
# make bin         compile the binary executable using optimizations
# make bint        same as 'make bin' plus moves a copy of the binary to the
#                  'testing/' folder
# make lib         create SOILWAT2 library
# make test        compile unit tests in 'test/ folder with googletest
# make test_run    run unit tests (in a previous step compiled with 'make test')
# make test_clean  delete test files and libraries
# make clean       delete all of the o files
# make cleaner     delete all of the o files, the shared object file(s), test files and
#                  libraries, and the binary exe
#-----------------------------------------------------------------------------------

uname_m = $(shell uname -m)

# CC = gcc
# CXX = g++
CFLAGS = -O3 -Wall -Wextra -pedantic
CXXFLAGS = -Wall -Wextra
LDFLAGS = -L.
LDLIBS = -lm -l$(target)

sources = SW_Main_lib.c SW_VegEstab.c SW_Control.c generic.c \
					rands.c Times.c mymemory.c filefuncs.c \
					SW_Files.c SW_Model.c SW_Site.c SW_SoilWater.c \
					SW_Markov.c SW_Weather.c SW_Sky.c SW_Output.c \
					SW_VegProd.c SW_Flow_lib.c SW_Flow.c
objects = $(sources:.c=.o)

bin_sources = SW_Main.c
bin_objects = $(bin_sources:.c=.o)

Rpkg_objects = $(objects) SW_R_lib.o SW_R_init.o

target = SOILWAT2
bin_test = sw_test
lib_target = lib$(target).a
SHLIB = r$(target)$(SHLIB_EXT)

gtest = gtest
lib_gtest = lib$(gtest).a
GTEST_DIR = googletest/googletest
GTEST_SRCS_ = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)
GTEST_HEADERS = $(GTEST_DIR)/include/gtest/*.h \
                $(GTEST_DIR)/include/gtest/internal/*.h



all: $(SHLIB)
$(SHLIB) :
		R CMD SHLIB -o $(SHLIB) $(Rpkg_objects)
		@rm -f $(Rpkg_objects)


.PHONY : lib bin bint binl
lib : $(lib_target)

$(lib_target) :
		$(CC) $(CFLAGS) -c $(sources)
		ar -rcsu $(lib_target) $(objects)
		@rm -f $(objects)

bin : $(target)

$(target) : $(lib_target)
		$(CC) $(CFLAGS) $(LDFLAGS) -o $(target) $(bin_sources) $(LDLIBS)

bint : $(target)
		cp $(target) testing/$(target)


.PHONY : test_lib test test_run
		# based on section 'Generic Build Instructions' in https://github.com/google/googletest/tree/master/googletest)
		#   1) build googletest library
		#   2) compile SOILWAT2 test source file
lib_test : $(lib_gtest)

$(lib_gtest) :
		$(CXX) $(CXXFLAGS) -isystem ${GTEST_DIR}/include -I${GTEST_DIR} \
			-pthread -c ${GTEST_DIR}/src/gtest-all.cc
		ar -r $(lib_gtest) gtest-all.o

test : $(lib_gtest) $(lib_target)
		$(CXX) $(CXXFLAGS)  $(LDFLAGS) -isystem ${GTEST_DIR}/include -pthread \
				$(lib_gtest) test/*.cc -o $(bin_test) $(LDLIBS)

test_run :
		./$(bin_test)


.PHONY : clean clean2 test_clean cleaner
clean :
		@rm -f $(objects) $(bin_objects)

clean2 :
		@rm -f $(target) $(SHLIB) $(lib_target)
		@rm -f testing/$(target)
		@rm -f testing/Output/*

test_clean :
		@rm -f gtest-all.o $(lib_gtest) $(bin_test)

cleaner : clean clean2 test_clean
