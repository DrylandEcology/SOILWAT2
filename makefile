#-----------------------------------------------------------------------------------
# 05/24/2012 (DLM)
# 07/14/2017 (drs)
# for use in terminal while in the project source directory to make compiling easier
#-----------------------------------------------------------------------------------
# commands          explanations
#-----------------------------------------------------------------------------------
# make             make the exe and all the o (object) files
# make all
# make compile     compile the exe using optimizations (no making of o files), for
#                  outputting finished version (exe smaller in size & most-likely faster
#                  then the version produced by the make), gives warnings as well
# make compilet    same as make compile except also makes a copy of the exe
#                  (named test) and moves it to the 'testing/' folder
# make gtest       compile unit tests in 'test/ folder with googltest
# make compilel    is used for compiling on Linux (explicitly link output against
#                  GCC's libmath library)
# make clean       delete all of the o files
# make gtest_clean delete test files and libraries
# make cleaner     delete all of the o files, the shared object file(s), test files and libraries, and the exe
#-----------------------------------------------------------------------------------

pkg = rSOILWAT2

sources = SW_Main.c SW_VegEstab.c SW_Control.c generic.c \
					rands.c Times.c mymemory.c filefuncs.c \
					SW_Files.c SW_Model.c SW_Site.c SW_SoilWater.c \
					SW_Markov.c SW_Weather.c SW_Sky.c SW_Output.c \
					SW_VegProd.c SW_Flow_lib.c SW_Flow.c SW_Carbon.c

objects = $(sources:.c=.o)

OBJECTS = $(objects) SW_R_lib.o SW_R_init.o

target_exe = sw_v31
SHLIB = $(pkg)$(SHLIB_EXT)
GTEST_DIR = googletest/googletest

all: $(SHLIB)
$(SHLIB) :
		R CMD SHLIB -o $(SHLIB) $(OBJECTS)
		@rm -f $(OBJECTS)

.PHONY : clean
clean :
		@rm -f $(OBJECTS)

.PHONY : gtest_clean
gtest_clean :
		@rm -f libgtest.a gtest-all.o sw_test

.PHONY : cleaner
cleaner :
		@rm -f $(OBJECTS) $(target_exe) $(pkg).so $(pkg).dll
		@rm -f testing/test
		@rm -f testing/Output/*
		@rm -f libgtest.a gtest-all.o sw_test

.PHONY : compile
compile :
		gcc -O3 -Wall -Wextra -o $(target_exe) $(sources)

.PHONY : compilet
compilet :
		gcc -O3 -Wall -Wextra -o $(target_exe) $(sources)
		cp $(target_exe) testing/test

.PHONY : compilel
compilel :
		gcc -O3 -Wall -Wextra -o $(target_exe) $(sources) -lm

.PHONY : gtest
		# based on section 'Generic Build Instructions' in https://github.com/google/googletest/tree/master/googletest)
		#   1) build googletest library
		#   2) compile SOILWAT2 test source file
gtest :
		g++ -isystem ${GTEST_DIR}/include -I${GTEST_DIR} \
			-pthread -c ${GTEST_DIR}/src/gtest-all.cc
		ar -rv libgtest.a gtest-all.o
		g++ -isystem ${GTEST_DIR}/include -pthread test/sw_test.cc libgtest.a -o sw_test
