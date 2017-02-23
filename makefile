#-----------------------------------------------------------------------------------
# 05/24/2012 (DLM)
# for use in terminal while in the project source directory to make compiling easier
#-----------------------------------------------------------------------------------
# commands					explanations
#-----------------------------------------------------------------------------------
# make 						make the exe and all the o (object) files
# make compile 				compile the exe using optimizations (no making of o files), for outputting finished version (exe smaller in size & most-likely faster then the version produced by the make), gives warnings as well
# make compilem				same as make compile except also makes a copy of the exe (named test) and moves it to the /testing folder
# make compilej				is used for compiling the exe on JANUS
# make compilel				is used for compiling on Linux (explicitly link output against GCC's libmath library)
# make clean 				delete the exe and all of the o files
# make cleano 				delete all of the o files, but not the exe
# make cleaner 				delete all of the o and so files (use when compiling for R package)
#-----------------------------------------------------------------------------------

objects = SW_Main.o SW_VegEstab.o SW_Control.o generic.o \
          rands.o Times.o mymemory.o filefuncs.o \
          SW_Files.o SW_Model.o SW_Site.o SW_SoilWater.o \
          SW_Markov.o SW_Weather.o SW_Sky.o SW_Output.o \
          SW_VegProd.o SW_Flow_lib.o SW_Flow.o

OBJECTS = SW_Main.o SW_VegEstab.o SW_Control.o generic.o \
          rands.o Times.o mymemory.o filefuncs.o \
          SW_Files.o SW_Model.o SW_Site.o SW_SoilWater.o \
          SW_Markov.o SW_Weather.o SW_Sky.o SW_Output.o \
          SW_VegProd.o SW_Flow_lib.o SW_Flow.o SW_R_lib.o

target_exe = sw_v31

target_r = rSOILWAT2.so

all:
	R CMD SHLIB -o $(target_r) $(OBJECTS)

test : $(objects)
		gcc -o $(target_exe) $(objects)

SW_Main.o : generic.h filefuncs.h SW_Defines.h SW_Control.h
SW_VegEstab.o : generic.h filefuncs.h myMemory.h SW_Defines.h SW_Files.h SW_Site.h SW_Times.h SW_Model.h SW_SoilWater.h SW_Weather.h SW_VegEstab.h
SW_Control.o :  generic.h filefuncs.h rands.h SW_Defines.h SW_Files.h SW_Control.h SW_Model.h SW_Output.h SW_Site.h SW_SoilWater.h SW_VegEstab.h SW_VegProd.h SW_Weather.h
generic.o : generic.h filefuncs.h
rands.o : generic.h rands.h myMemory.h
Times.o : generic.h Times.h
mymemory.o : generic.h myMemory.h
filefuncs.o : filefuncs.h myMemory.h
SW_Files.o : generic.h filefuncs.h myMemory.h SW_Defines.h SW_Files.h
SW_Model.o : generic.h filefuncs.h rands.h Times.h SW_Defines.h SW_Files.h SW_Weather.h SW_Site.h SW_SoilWater.h SW_Times.h SW_Model.h
SW_Site.o : generic.h filefuncs.h myMemory.h SW_Defines.h SW_Files.h SW_Site.h SW_SoilWater.h
SW_SoilWater.o : generic.h filefuncs.h myMemory.h SW_Defines.h SW_Files.h SW_Model.h SW_Site.h SW_SoilWater.h SW_Output.h SW_VegProd.h
SW_Markov.o : generic.h filefuncs.h rands.h myMemory.h SW_Defines.h SW_Files.h SW_Weather.h SW_Model.h SW_Markov.h
SW_Weather.o : generic.h filefuncs.h myMemory.h Times.h SW_Defines.h SW_Files.h SW_Model.h SW_Output.h SW_SoilWater.h SW_Weather.h SW_Markov.h SW_Sky.h
SW_Sky.o : generic.h filefuncs.h SW_Defines.h SW_Files.h SW_Sky.h
SW_Output.o : generic.h filefuncs.h myMemory.h Times.h SW_Defines.h SW_Files.h SW_Model.h SW_Site.h SW_SoilWater.h SW_Times.h SW_Output.h SW_Weather.h SW_VegEstab.h
SW_VegProd.o : generic.h filefuncs.h SW_Defines.h SW_Files.h SW_Times.h SW_VegProd.h
SW_Flow_lib.o : generic.h SW_Defines.h SW_Flow_lib.h SW_Flow_subs.h Times.h
SW_Flow.o : generic.h filefuncs.h SW_Defines.h SW_Model.h SW_Site.h SW_SoilWater.h SW_Flow_lib.h SW_VegProd.h SW_Weather.h SW_Sky.h

.PHONY : compile
compile :
		gcc -O3 -Wall -Wextra -o $(target_exe) SW_Main.c SW_VegEstab.c SW_Control.c generic.c rands.c Times.c mymemory.c filefuncs.c SW_Files.c SW_Model.c SW_Site.c SW_SoilWater.c SW_Markov.c SW_Weather.c SW_Sky.c SW_Output.c SW_VegProd.c SW_Flow_lib.c SW_Flow.c

.PHONY : compilem
compilem :
		gcc -O3 -Wall -Wextra -o $(target_exe) SW_Main.c SW_VegEstab.c SW_Control.c generic.c rands.c Times.c mymemory.c filefuncs.c SW_Files.c SW_Model.c SW_Site.c SW_SoilWater.c SW_Markov.c SW_Weather.c SW_Sky.c SW_Output.c SW_VegProd.c SW_Flow_lib.c SW_Flow.c
		cp $(target_exe) test
		mv test testing/test

.PHONY : compilej
compilej :
		mpicc -o $(target_exe) SW_Main.c SW_VegEstab.c SW_Control.c generic.c rands.c Times.c mymemory.c filefuncs.c SW_Files.c SW_Model.c SW_Site.c SW_SoilWater.c SW_Markov.c SW_Weather.c SW_Sky.c SW_Output.c SW_VegProd.c SW_Flow_lib.c SW_Flow.c

.PHONY : clean
clean :
		-rm $(target_exe) $(objects)

.PHONY : cleano
cleano :
		-rm $(objects)

.PHONY : cleaner
cleaner :
		-rm $(target_r) SW_R_lib.o $(objects)

.PHONY : compilel
compilel :
		gcc -O3 -Wall -Wextra -o $(target_exe) SW_Main.c SW_VegEstab.c SW_Control.c generic.c rands.c Times.c mymemory.c filefuncs.c SW_Files.c SW_Model.c SW_Site.c SW_SoilWater.c SW_Markov.c SW_Weather.c SW_Sky.c SW_Output.c SW_VegProd.c SW_Flow_lib.c SW_Flow.c -lm
		cp $(target_exe) testing
