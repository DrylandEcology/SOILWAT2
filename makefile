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
# make binl        same as 'make bin', but for compiling on Linux (i.e., explicitly link
#                  output against GCC's libmath library)
# make test        compile unit tests in 'test/ folder with googletest
# make test_run    run unit tests (in a previous step compiled with 'make test')
# make test_runWin run unit tests on a Windows OS (in a previous step compiled with
#                  'make test')
# make test_clean  delete test files and libraries
# make clean       delete all of the o files
# make cleaner     delete all of the o files, the shared object file(s), test files and
#                  libraries, and the binary exe
#-----------------------------------------------------------------------------------

pkg = rSOILWAT2

sources = SW_Main.c SW_VegEstab.c SW_Control.c generic.c \
					rands.c Times.c mymemory.c filefuncs.c \
					SW_Files.c SW_Model.c SW_Site.c SW_SoilWater.c \
					SW_Markov.c SW_Weather.c SW_Sky.c SW_Output.c \
					SW_VegProd.c SW_Flow_lib.c SW_Flow.c

objects = $(sources:.c=.o)

OBJECTS = $(objects) SW_R_lib.o SW_R_init.o

bin_target = sw_v31
bin_test = sw_test
SHLIB = $(pkg)$(SHLIB_EXT)
GTEST_DIR = googletest/googletest
GTEST_SRCS_ = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)
GTEST_HEADERS = $(GTEST_DIR)/include/gtest/*.h \
                $(GTEST_DIR)/include/gtest/internal/*.h


all: $(SHLIB)
$(SHLIB) :
		R CMD SHLIB -o $(SHLIB) $(OBJECTS)
		@rm -f $(OBJECTS)

.PHONY : bin bint binl
bin :
		gcc -O3 -Wall -Wextra -o $(bin_target) $(sources)

bint :
		gcc -O3 -Wall -Wextra -o $(bin_target) $(sources)
		cp $(bin_target) testing/$(bin_target)

binl :
		gcc -O3 -Wall -Wextra -o $(bin_target) $(sources) -lm


.PHONY : test test_run test_runWin
		# based on section 'Generic Build Instructions' in https://github.com/google/googletest/tree/master/googletest)
		#   1) build googletest library
		#   2) compile SOILWAT2 test source file
test : $(GTEST_SRCS_)
		g++ -isystem ${GTEST_DIR}/include -I${GTEST_DIR} \
			-pthread -c ${GTEST_DIR}/src/gtest-all.cc
		ar -rv libgtest.a gtest-all.o
		g++ -isystem ${GTEST_DIR}/include -pthread test/*.cc libgtest.a -o $(bin_test)

test_run :
		./$(bin_test)

test_runWin :
		$(bin_test).exe


.PHONY : clean clean2 test_clean cleaner
clean :
		@rm -f $(OBJECTS)

clean2 :
		@rm -f $(bin_target) $(pkg).so $(pkg).dll
		@rm -f testing/$(bin_target)
		@rm -f testing/Output/*

test_clean :
		@rm -f libgtest.a gtest-all.o $(bin_test)

cleaner : clean clean2 test_clean
