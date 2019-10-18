#!/bin/bash

sources_tests='SW_Main_lib.c SW_VegEstab.c SW_Control.c generic.c
					rands.c Times.c mymemory.c filefuncs.c
					SW_Files.c SW_Model.c SW_Site.c SW_SoilWater.c
					SW_Markov.c SW_Weather.c SW_Sky.c SW_Output_mock.c
					SW_VegProd.c SW_Flow_lib.c SW_Flow.c SW_Carbon.c'

for sf in $sources_tests
do
  gcov $sf
done
