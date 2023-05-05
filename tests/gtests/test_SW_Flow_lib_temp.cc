#include "gtest/gtest.h"
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <typeinfo>  // for 'typeid'

#include "include/generic.h"
#include "include/myMemory.h"
#include "include/filefuncs.h"
#include "include/rands.h"
#include "include/Times.h"
#include "include/SW_Defines.h"
#include "include/SW_Times.h"
#include "include/SW_Files.h"
#include "include/SW_Carbon.h"
#include "include/SW_Site.h"
#include "include/SW_VegProd.h"
#include "include/SW_VegEstab.h"
#include "include/SW_Model.h"
#include "include/SW_SoilWater.h"
#include "include/SW_Weather.h"
#include "include/SW_Markov.h"
#include "include/SW_Sky.h"
#include "external/pcg/pcg_basic.h"

#include "include/SW_Flow_lib.h"

#include "tests/gtests/sw_testhelpers.h"


pcg32_random_t flowTemp_rng;

namespace {

  // Test the function 'surface_temperature_under_snow'
  TEST(SWFlowTempTest, SurfaceTemperatureUnderSnow) {

    // declare inputs and output
    double snow, airTempAvg;
    double tSoilAvg;

    // test when snow is 0 and airTempAvg > 0
    snow = 0, airTempAvg = 10;

    tSoilAvg = surface_temperature_under_snow(airTempAvg, snow);

    EXPECT_EQ(0, tSoilAvg); // When there is snow, the return is 0


    // test when snow is > 0 and airTempAvg is >= 0
    snow = 1, airTempAvg = 0;

    tSoilAvg = surface_temperature_under_snow(airTempAvg, snow);

    EXPECT_EQ(-2.0, tSoilAvg); // When there is snow and airTemp >= 0, the return is -2.0

    // test when snow > 0 and airTempAvg < 0
    snow = 1, airTempAvg = -10;

    tSoilAvg = surface_temperature_under_snow(airTempAvg, snow);

    EXPECT_EQ(-4.55, tSoilAvg); // When there snow == 1 airTempAvg = -10

    //
    snow = 6.7, airTempAvg = 0;

    tSoilAvg = surface_temperature_under_snow(airTempAvg, snow);

    EXPECT_EQ(-2.0, tSoilAvg); // When there is snow > 6.665 and airTemp >= 0, the return is -2.0

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }

  // Test the soil temperature initialization function 'soil_temperature_setup'
  TEST(SWFlowTempTest, SoilTemperatureInit) {

    // declare inputs and output
    double deltaX = 15.0, theMaxDepth = 990.0, sTconst = 4.15;
    unsigned int nlyrs, nRgr = 65;
    Bool ptr_stError = swFALSE;
    pcg32_random_t STInit_rng;
    RandSeed(0u, 0u, &STInit_rng);

    // *****  Test when nlyrs = 1  ***** //
    unsigned int i =0.;
    nlyrs = 1;
    double width[] = {20}, oldsTemp[] = {1};
    double bDensity[] = {RandNorm(1.,0.5,&STInit_rng)}, fc[] = {RandNorm(1.5, 0.5,&STInit_rng)};
    double wp[1];
    wp[0]= fc[0] - 0.6; // wp will always be less than fc

    /// test standard conditions
    soil_temperature_setup(bDensity, width, oldsTemp, sTconst, nlyrs,
      fc, wp, deltaX, theMaxDepth, nRgr, &ptr_stError);

    //Structure Tests
    EXPECT_EQ(sizeof(stValues.tlyrs_by_slyrs),
      sizeof(double) * MAX_ST_RGR * (MAX_LAYERS + 1));

    for(unsigned int i = ceil(stValues.depths[nlyrs - 1]/deltaX); i < nRgr + 1; i++){
        EXPECT_EQ(stValues.tlyrs_by_slyrs[i][nlyrs], -deltaX);
        //Values should be equal to -deltaX when i > the depth of the soil profile/deltaX and j is == nlyrs
    }

    // Other init test
    EXPECT_EQ(stValues.depths[nlyrs - 1], 20); // sum of inputs width = maximum depth; in my example 20
    EXPECT_EQ((stValues.depthsR[nRgr]/deltaX) - 1, nRgr); // nRgr = (MaxDepth/deltaX) - 1

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();


    // *****  Test when nlyrs = MAX_LAYERS (SW_Defines.h)  ***** //
    /// generate inputs using a for loop
    nlyrs = MAX_LAYERS;
    double width2[] = {5, 5, 5, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 20, 20, 20, 20, 20, 20};
    double oldsTemp2[] = {1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4};
    double *bDensity2 = new double[nlyrs];
    double *fc2 = new double[nlyrs];
    double *wp2 = new double[nlyrs];

    for (i = 0; i < nlyrs; i++) {
      bDensity2[i] = RandNorm(1.,0.5,&STInit_rng);
      fc2[i] = RandNorm(1.5, 0.5,&STInit_rng);
      wp2[i] = fc2[i] - 0.6; // wp will always be less than fc
    }

    soil_temperature_setup(bDensity2, width2, oldsTemp2, sTconst, nlyrs,
      fc2, wp2, deltaX, theMaxDepth, nRgr, &ptr_stError);

    //Structure Tests
    EXPECT_EQ(sizeof(stValues.tlyrs_by_slyrs),
      sizeof(double) * MAX_ST_RGR * (MAX_LAYERS + 1));

    for(unsigned int i = ceil(stValues.depths[nlyrs - 1]/deltaX); i < nRgr + 1; i++){
        EXPECT_EQ(stValues.tlyrs_by_slyrs[i][nlyrs], -deltaX);
        //Values should be equal to -deltaX when i > the depth of the soil profile/deltaX and j is == nlyrs
    }

    // Other init test
    EXPECT_EQ(stValues.depths[nlyrs - 1], 295); // sum of inputs width = maximum depth; in my example 295
    EXPECT_EQ((stValues.depthsR[nRgr]/deltaX) - 1, nRgr); // nRgr = (MaxDepth/deltaX) - 1

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
    delete[] bDensity2; delete[] fc2; delete[] wp2;
  }

  // Death tests for soil_temperature_setup function
  TEST(SWFlowTempDeathTest, SoilTemperatureInit) {

    // *****  Test when nlyrs = MAX_LAYERS (SW_Defines.h)  ***** //
    double deltaX = 15.0, sTconst = 4.15;
    unsigned int nlyrs, nRgr = 65, i =0.;
    Bool ptr_stError = swFALSE;
    nlyrs = MAX_LAYERS;
    double width2[] = {5, 5, 5, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 20, 20, 20, 20, 20, 20};
    double oldsTemp2[] = {1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4};
    double *bDensity2 = new double[nlyrs];
    double *fc2 = new double[nlyrs];
    double *wp2 = new double[nlyrs];
    pcg32_random_t STInitDeath_rng;
    RandSeed(0u, 0u, &STInitDeath_rng);

    for (i = 0; i < nlyrs; i++) {
      bDensity2[i] = RandNorm(1.,0.5,&STInitDeath_rng);
      fc2[i] = RandNorm(1.5, 0.5,&STInitDeath_rng);
      wp2[i] = fc2[i] - 0.6; // wp will always be less than fc
    }

    /// test when theMaxDepth is less than soil layer depth - function should fail
    double theMaxDepth2 = 70.0;

    // We expect death when max depth < last layer
    EXPECT_DEATH_IF_SUPPORTED(
      soil_temperature_setup(
        bDensity2, width2, oldsTemp2, sTconst, nlyrs,
        fc2, wp2, deltaX, theMaxDepth2, nRgr,
        &ptr_stError
      ),
      "SOIL_TEMP FUNCTION ERROR: soil temperature max depth"
    );

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
    delete[] wp2; delete[] fc2; delete[] bDensity2;
  }


  // Test lyrSoil_to_lyrTemp, lyrSoil_to_lyrTemp_temperature via
  // soil_temperature_setup function
  TEST(SWFlowTempTest, SoilLayerInterpolationFunctions) {

    // declare inputs and output
    double deltaX = 15.0, theMaxDepth = 990.0, sTconst = 4.15;
    unsigned int nlyrs, nRgr = 65;
    Bool ptr_stError = swFALSE;

    pcg32_random_t SLIF_rng;
    RandSeed(0u, 0u, &SLIF_rng);

    // *****  Test when nlyrs = 1  ***** //
    unsigned int i = 0.;
    nlyrs = 1;
    double width[] = {20}, oldsTemp[] = {1};
    double bDensity[] = {fmaxf(RandNorm(1.5,0.5,&SLIF_rng), 0.1)},
      fc[] = {fmaxf(RandNorm(1.5, 0.5,&SLIF_rng), 0.1)};
    double wp[1];

    wp[0]= fmax(fc[0] - 0.6, .1); // wp will always be less than fc

    soil_temperature_setup(bDensity, width, oldsTemp, sTconst, nlyrs,
      fc, wp, deltaX, theMaxDepth, nRgr, &ptr_stError);

    // lyrSoil_to_lyrTemp tests: This function is used in soil_temperature_setup
    // to transfer the soil layer values of bdensity, fc, and wp, to the "temperature layer"
    // which are contained in bdensityR, fcR, and wpR. Thus we check these values.
    for (i = 0; i < nRgr + 1; i++) {  // all Values should be greater than 0
      EXPECT_GT(stValues.bDensityR[i], 0);
      EXPECT_GT(stValues.fcR[i], 0);
      EXPECT_GT(stValues.wpR[i], 0);
    }

    for (i = ceil(stValues.depths[nlyrs - 1]/deltaX) + 1; i < nRgr + 1; i++) {
      //The TempLayer values that are at depths greater than the max SoilLayer depth should be uniform
      EXPECT_EQ(stValues.bDensityR[i], stValues.bDensityR[i - 1]);
      EXPECT_EQ(stValues.fcR[i], stValues.fcR[i - 1]);
      EXPECT_EQ(stValues.wpR[i], stValues.wpR[i - 1]);
    }

    // lyrSoil_to_lyrTemp_temperature tests
    double maxvalR = 0.;
    for (i = 0; i < nRgr + 1; i++) {
      EXPECT_GT(stValues.oldavgLyrTempR[i], -100); //Values interpolated into oldsTempR should be realistic
      EXPECT_LT(stValues.oldavgLyrTempR[i], 100); //Values interpolated into oldsTempR should be realistic
      if(GT(stValues.oldavgLyrTempR[i], maxvalR)) {
        maxvalR = stValues.oldavgLyrTempR[i];
      }
    }
    EXPECT_LE(maxvalR, sTconst);//Maximum interpolated oldsTempR value should be less than or equal to maximum in oldsTemp2 (sTconst = last layer)
    EXPECT_EQ(stValues.oldavgLyrTempR[nRgr + 1], sTconst); //Temperature in last interpolated layer should equal sTconst

    // *****  Test when nlyrs = MAX_LAYERS (SW_Defines.h)  ***** //
    /// generate inputs using a for loop
    nlyrs = MAX_LAYERS;
    double width2[] = {5, 5, 5, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 20, 20, 20, 20, 20, 20};
    double oldsTemp2[] = {1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4};
    double *bDensity2 = new double[nlyrs];
    double *fc2 = new double[nlyrs];
    double *wp2 = new double[nlyrs];

    for (i = 0; i < nlyrs; i++) {
      bDensity2[i] = fmaxf(RandNorm(1.,0.5,&SLIF_rng), 0.1);
      fc2[i] = fmaxf(RandNorm(1.5, 0.5,&SLIF_rng), 0.1);
      wp2[i] = fmaxf(fc2[i] - 0.6, 0.1); // wp will always be less than fc
      EXPECT_GT(bDensity2[i], 0);
      EXPECT_GT(fc2[i], 0);
      EXPECT_GT(wp2[i], 0);
    }

    soil_temperature_setup(bDensity2, width2, oldsTemp2, sTconst, nlyrs,
      fc2, wp2, deltaX, theMaxDepth, nRgr, &ptr_stError);

    // lyrSoil_to_lyrTemp tests
    for (i = 0; i < nRgr + 1; i++) {  // all Values should be greater than 0
      EXPECT_GT(stValues.bDensityR[i], 0);
      EXPECT_GT(stValues.fcR[i], 0);
      EXPECT_GT(stValues.wpR[i], 0);
    }

    for (i = ceil(stValues.depths[nlyrs - 1]/deltaX) + 1; i < nRgr + 1; i++) {
      //The TempLayer values that are at depths greater than the max SoilLayer depth should be uniform
      EXPECT_EQ(stValues.bDensityR[i], stValues.bDensityR[i - 1]);
      EXPECT_EQ(stValues.fcR[i], stValues.fcR[i - 1]);
      EXPECT_EQ(stValues.wpR[i], stValues.wpR[i - 1]);
    }

    // lyrSoil_to_lyrTemp_temperature tests
    maxvalR = 0.;
    for (i = 0; i <= nRgr + 1; i++) {
      EXPECT_GT(stValues.oldavgLyrTempR[i], -200); //Values interpolated into oldsTempR should be realistic
      EXPECT_LT(stValues.oldavgLyrTempR[i], 200); //Values interpolated into oldsTempR should be realistic
      if(GT(stValues.oldavgLyrTempR[i], maxvalR)) {
        maxvalR = stValues.oldavgLyrTempR[i];
      }
    }
    EXPECT_LE(maxvalR, sTconst);//Maximum interpolated oldsTempR value should be less than or equal to maximum in oldsTemp2 (sTconst = last layer)
    EXPECT_EQ(stValues.oldavgLyrTempR[nRgr + 1], sTconst); //Temperature in last interpolated layer should equal sTconst

    //Reset to global state
    Reset_SOILWAT2_after_UnitTest();
    delete[] bDensity2; delete[] fc2; delete[] wp2;
  }

  // Test set layer to frozen or unfrozen 'set_frozen_unfrozen'
  TEST(SWFlowTempTest, SetFrozenUnfrozen){

    // declare inputs and output
    // *****  Test when nlyrs = 1  ***** //
    /// ***** Test that soil freezes ***** ///
    unsigned int nlyrs = 1;
    double sTemp[] = {-5}, swc[] = {1.5}, swc_sat[] = {1.8}, width[] = {5};

    set_frozen_unfrozen(nlyrs, sTemp, swc, swc_sat, width, SW_All.SoilWat.lyrFrozen);

    EXPECT_EQ(1,SW_All.SoilWat.lyrFrozen[0]); // Soil should freeze when sTemp is <= -1
    // AND swc is > swc_sat - width * .13

    /// ***** Test that soil does not freeze ***** ///
    double sTemp2[] = {0};

    set_frozen_unfrozen(nlyrs, sTemp2, swc, swc_sat, width, SW_All.SoilWat.lyrFrozen);

    EXPECT_EQ(0,SW_All.SoilWat.lyrFrozen[0]); // Soil should NOT freeze when sTemp is > -1

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();

    // *****  Test when nlyrs = MAX_LAYERS (SW_Defines.h)  ***** //
    nlyrs = MAX_LAYERS;
    double width2[] = {5, 5, 5, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 20, 20, 20, 20, 20, 20};
    double *sTemp3 = new double[nlyrs];
    double *sTemp4 = new double[nlyrs];
    double *swc2 = new double[nlyrs];
    double *swc_sat2 = new double[nlyrs];

    unsigned int i = 0.;
    for (i = 0; i < nlyrs; i++) {
      sTemp3[i] = -5;
      sTemp4[i] = 0;
      swc2[i] = 5; // set swc to a high value so will be > swc_sat - width * .13
      swc_sat2[i] = 1;
      // run
      set_frozen_unfrozen(nlyrs, sTemp3, swc2, swc_sat2, width2, SW_All.SoilWat.lyrFrozen);
      // Test
      EXPECT_EQ(1,SW_All.SoilWat.lyrFrozen[i]);
      // run
      set_frozen_unfrozen(nlyrs, sTemp4, swc2, swc_sat2, width2, SW_All.SoilWat.lyrFrozen);
      // Test
      EXPECT_EQ(0,SW_All.SoilWat.lyrFrozen[i]);
    }
    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
    delete[] sTemp3; delete[] sTemp4; delete[] swc2; delete[] swc_sat2;
  }

  // Test soil temperature today function 'soil_temperature_today'
  TEST(SWFlowTempTest, SoilTemperatureTodayFunction) {

    // declare inputs and output
    double delta_time = 86400., deltaX = 15.0, T1 = 20.0, sTconst = 4.16, csParam1 = 0.00070,
    csParam2 = 0.000030, shParam = 0.18, surface_range = 1.;
    unsigned int nRgr =65;
    Bool ptr_stError = swFALSE;

    pcg32_random_t STTF_rng;
    RandSeed(0u, 0u, &STTF_rng);

    // declare input in for loop for non-error causing conditions;
    /// don't use RandNorm for fcR, wpR, vwcR, and bDensityR because will trigger
    /// error causing condtions

    double *sTempR = new double[nRgr + 2];
    double *oldsTempR = new double[nRgr + 2];
    double *wpR = new double[nRgr + 2];
    double *fcR = new double[nRgr + 2];
    double *vwcR = new double[nRgr + 2];
    double *bDensityR = new double[nRgr + 2];
    double *temperatureRangeR = new double[nRgr + 2];
    double *depthsR = new double[nRgr + 2];
    unsigned int i = 0.;
    for (i = 0; i <= nRgr + 1; i++) {
      sTempR[i] = RandNorm(1.5, 1,&STTF_rng);
      oldsTempR[i] = RandNorm(1.5, 1,&STTF_rng);
      fcR[i] = 2.1;
      wpR[i] = 1.5; // wp will always be less than fc
      vwcR[i] = 1.6;
      bDensityR[i] = 1.5;
    }

    soil_temperature_today(&delta_time, deltaX, T1, sTconst, nRgr, sTempR, oldsTempR,
      vwcR, wpR, fcR, bDensityR, csParam1, csParam2, shParam, &ptr_stError, surface_range,
      temperatureRangeR, depthsR, SW_All.Model.year, SW_All.Model.doy);

    // Check that values that are set, are set right.
    EXPECT_EQ(sTempR[0], T1);
    EXPECT_EQ(sTempR[nRgr + 1], sTconst);

    //Check that ptr_stError is FALSE
    EXPECT_EQ(ptr_stError, 0);

    //Check that when  ptr_stError is FALSE, sTempR values are realisitic and pass check in code (between -100 and 100)
    for (i = 0; i <= nRgr + 1; i++)
    {
      EXPECT_LT(sTempR[i], 100);
      EXPECT_GT(sTempR[i], -100);
    }

    // test that the ptr_stError is FALSE when it is supposed to
    double *sTempR2 = new double[nRgr + 2];
    double *oldsTempR3 = new double[nRgr + 2];

    for (i = 0; i <= nRgr + 1; i++)
    {
      sTempR2[i] = RandNorm(150, 1,&STTF_rng);
      oldsTempR3[i] = RandNorm(150, 1,&STTF_rng);
    }

    soil_temperature_today(&delta_time, deltaX, T1, sTconst, nRgr, sTempR2, oldsTempR3,
        vwcR, wpR, fcR, bDensityR, csParam1, csParam2, shParam, &ptr_stError, surface_range,
        temperatureRangeR, depthsR, SW_All.Model.year, SW_All.Model.doy);

    //Check that ptr_stError is TRUE
    EXPECT_EQ(ptr_stError, 1);
    double *array_list[] = {sTempR2, oldsTempR3, sTempR, oldsTempR, wpR, fcR, vwcR, bDensityR, temperatureRangeR, depthsR};
    for (i = 0; i < length(array_list); i++){
      delete[] array_list[i];
    }
    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }

  // Test main soil temperature function 'soil_temperature'
  // AND lyrTemp_to_lyrSoil_temperature as this function
  // is only called in the soil_temperature function
  TEST(SWFlowTempTest, MainSoilTemperatureFunction_Lyr01) {

    unsigned int k, i;

    pcg32_random_t MSTF_Lyer1_rng;
    RandSeed(0u, 0u, &MSTF_Lyer1_rng);

    // *****  Test when nlyrs = 1  ***** //
    unsigned int nlyrs = 1, nRgr = 65;
    double airTemp = 25.0, pet = 5.0, aet = 4.0, biomass = 100., surfaceTemp = 15.,
    bmLimiter = 300., t1Param1 = 15., t1Param2 = -4., t1Param3 = 600., csParam1 =0.00070,
    csParam2 = 0.00030, shParam = 0.18, snowdepth = 5, sTconst = 4.15, deltaX = 15,
    theMaxDepth = 990., snow = 1, max_air_temp = 10.1, min_air_temp = -5.0, H_gt = 300.0,
    surface_max = 10.6, surface_min = -6.8;
    Bool ptr_stError = swFALSE;

    double swc[] = {1.0}, swc_sat[] = {1.5}, bDensity[] = {1.8}, width[] = {20},
    oldsTemp[] = {5.0}, sTemp[1], fc[] = {2.6}, wp[] = {1.0}, min_temp[] = {10.0}, max_temp[] = {1.0};


    SW_ST_setup_run(
      airTemp,
      swc, swc_sat,
      bDensity, width,
      oldsTemp,
      &surfaceTemp,
      nlyrs, fc, wp,
      sTconst, deltaX, theMaxDepth,
      nRgr, &ptr_stError, SW_All.SoilWat.lyrFrozen
    );

    for (k = 0; k < nlyrs; k++) {
      sTemp[k] = oldsTemp[k];
    }

    soil_temperature(&surface_max, &surface_min, SW_All.SoilWat.lyrFrozen,
      airTemp, pet, aet, biomass, swc, swc_sat, bDensity, width,
      sTemp, &surfaceTemp, nlyrs, bmLimiter, t1Param1, t1Param2,
      t1Param3, csParam1, csParam2, shParam, snowdepth, sTconst, deltaX,
      theMaxDepth, nRgr, snow, max_air_temp, min_air_temp, H_gt,
      SW_All.Model.year, SW_All.Model.doy, min_temp, max_temp, &ptr_stError);


    // Expect that surface temp equals surface_temperature_under_snow() because snow > 0
    EXPECT_EQ(surfaceTemp, surface_temperature_under_snow(airTemp, snow));
    EXPECT_NE(surfaceTemp, airTemp + ((t1Param2 * (biomass - bmLimiter)) / t1Param3));
    EXPECT_NE(surfaceTemp, airTemp + (t1Param1 * pet * (1. - (aet / pet)) * (1. - (biomass / bmLimiter))));

    // Test surface temp equals equation when biomass < blimititer & snow = 0
    snowdepth = 0;

    for (k = 0; k < nlyrs; k++) {
      sTemp[k] = oldsTemp[k];
    }

    soil_temperature(&surface_max, &surface_min, SW_All.SoilWat.lyrFrozen,
      airTemp, pet, aet, biomass, swc, swc_sat, bDensity, width,
      sTemp, &surfaceTemp, nlyrs, bmLimiter, t1Param1, t1Param2,
      t1Param3, csParam1, csParam2, shParam, snowdepth, sTconst, deltaX,
      theMaxDepth, nRgr, snow, max_air_temp, min_air_temp, H_gt,
      SW_All.Model.year, SW_All.Model.doy, min_temp, max_temp, &ptr_stError);

    EXPECT_EQ(surfaceTemp, airTemp + (t1Param1 * pet * (1. - (aet / pet)) * (1. - (biomass / bmLimiter))));
    EXPECT_NE(surfaceTemp, airTemp + ((t1Param2 * (biomass - bmLimiter)) / t1Param3));
    EXPECT_NE(surfaceTemp, surface_temperature_under_snow(airTemp, snow));

    //Test surface temp equals equation when biomass > blimititer & snow = 0
    biomass = 305;

    for (k = 0; k < nlyrs; k++) {
      sTemp[k] = oldsTemp[k];
    }

    soil_temperature(&surface_max, &surface_min, SW_All.SoilWat.lyrFrozen,
      airTemp, pet, aet, biomass, swc, swc_sat, bDensity, width,
      sTemp, &surfaceTemp, nlyrs, bmLimiter, t1Param1, t1Param2,
      t1Param3, csParam1, csParam2, shParam, snowdepth, sTconst, deltaX,
      theMaxDepth, nRgr, snow, max_air_temp, min_air_temp, H_gt,
      SW_All.Model.year, SW_All.Model.doy, min_temp, max_temp, &ptr_stError);

    EXPECT_EQ(surfaceTemp, airTemp + ((t1Param2 * (biomass - bmLimiter)) / t1Param3));
    EXPECT_NE(surfaceTemp, airTemp + (t1Param1 * pet * (1. - (aet / pet)) * (1. - (biomass / bmLimiter))));
    EXPECT_NE(surfaceTemp, surface_temperature_under_snow(airTemp, snow));

    // checks for  lyrTemp_to_lyrSoil_temperature
    int resultValue = sizeof(sTemp) / sizeof(sTemp[0]);
    EXPECT_EQ(1, resultValue); // when the number of soil layers is 1, sTemp should have length 1
    EXPECT_GT(sTemp[0], -100); // Sense check
    EXPECT_LT(sTemp[0], 100); // Sense check
    EXPECT_EQ(0, ptr_stError); // ptr_stError should be FALSE

    // Expect that oldsTempR is updated to sTempR for the next day
    for (k = 0; k <= nRgr + 1; k++)
    {
      //swprintf("\n k %u, newoldtempR %f", k, stValues.oldavgLyrTempR[k]);
      EXPECT_NE(stValues.oldavgLyrTempR[k], SW_MISSING);
    }

    //Reset to global state
    Reset_SOILWAT2_after_UnitTest();


    // ptr_stError should be set to TRUE if soil_temperature_today fails (i.e. unrealistic temp values)
    double *sTemp2 = new double[nlyrs];
    double *oldsTemp2 = new double[nlyrs];

    surfaceTemp = airTemp = 1500.;

    for (i = 0; i < nlyrs; i++)
    {
      sTemp2[i] = RandNorm(surfaceTemp, 1, &MSTF_Lyer1_rng);
      oldsTemp2[i] = RandNorm(surfaceTemp, 1, &MSTF_Lyer1_rng);
    }

    SW_ST_setup_run(
      airTemp,
      swc, swc_sat,
      bDensity, width,
      oldsTemp,
      &surfaceTemp,
      nlyrs, fc, wp,
      sTconst, deltaX, theMaxDepth,
      nRgr, &ptr_stError, SW_All.SoilWat.lyrFrozen
    );

    EXPECT_EQ(ptr_stError, swFALSE);

    soil_temperature(&surface_max, &surface_max, SW_All.SoilWat.lyrFrozen,
      airTemp, pet, aet, biomass, swc, swc_sat, bDensity, width,
      sTemp2, &surfaceTemp, nlyrs, bmLimiter, t1Param1, t1Param2, t1Param3,
      csParam1, csParam2, shParam, snowdepth, sTconst, deltaX, theMaxDepth,
      nRgr, snow, max_air_temp, min_air_temp, H_gt, SW_All.Model.year,
      SW_All.Model.doy, min_temp, max_temp, &ptr_stError);

    // Check that error has occurred as indicated by ptr_stError
    EXPECT_EQ(ptr_stError, swTRUE);

    //Reset to global state
    Reset_SOILWAT2_after_UnitTest();
    delete[] oldsTemp2; delete[] sTemp2;
  }

  // Test main soil temperature function 'soil_temperature'
  // AND lyrTemp_to_lyrSoil_temperature as this function
  // is only called in the soil_temperature function
  TEST(SWFlowTempTest, MainSoilTemperatureFunction_LyrMAX) {
    // *****  Test when nlyrs = MAX_LAYERS  ***** //
    pcg32_random_t soilTemp_rng;
    RandSeed(0u, 0u, &soilTemp_rng);


    unsigned int i, k;

    // intialize values
    unsigned int nRgr = 65;
    double airTemp = 25.0, pet = 5.0, aet = 4.0, biomass = 100., surfaceTemp = 15.,
    bmLimiter = 300., t1Param1 = 15., t1Param2 = -4., t1Param3 = 600., csParam1 =0.00070,
    csParam2 = 0.00030, shParam = 0.18, snowdepth = 5, sTconst = 4.15, deltaX = 15,
    theMaxDepth = 990., snow = 1, max_air_temp = 10.1, min_air_temp = -5.0, H_gt = 300.0,
    surface_max = 10.6, surface_min = -6.8;
    Bool ptr_stError = swFALSE;

    unsigned int nlyrs2 = MAX_LAYERS;
    double width2[] = {5, 5, 5, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 20, 20, 20, 20, 20, 20};
    double oldsTemp3[] = {1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4};
    double sTemp3[MAX_LAYERS];
    double bDensity2[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

    // we don't need soil texture, but we need SWC(sat), SWC(field capacity),
    // and SWC(wilting point)
    double *swc2 = new double[nlyrs2];
    double *swc_sat2 = new double[nlyrs2];
    double *fc2 = new double[nlyrs2];
    double *wp2 = new double[nlyrs2];
    double *min_temp = new double[nlyrs2];
    double *max_temp = new double[nlyrs2];
    for (i = 0; i < nlyrs2; i++) {
      // SWC(wilting point): width > swc_wp > 0
      wp2[i] = 0.1 * width2[i];
      // SWC(field capacity): width > swc_fc > swc_wp
      fc2[i] = fminf(width2[i], wp2[i] + 0.15 * width2[i]);
      // SWC(saturation): width > swc_sat > swc_fc
      swc_sat2[i] = fminf(width2[i], fc2[i] + 0.2 * width2[i]);
      // SWC: swc_sat >= SWC > 0; here, swc_fc >= SWC >= swc_wp
      swc2[i] = RandUniFloatRange(wp2[i], fc2[i], &soilTemp_rng);

      //swprintf("\n i %u, bDensity %f, swc_sat %f, fc %f, swc %f,  wp %f",
      //  i, bDensity2[i],  swc_sat2[i], fc2[i], swc2[i], wp2[i] );
    }

    SW_ST_setup_run(
      airTemp,
      swc2, swc_sat2,
      bDensity2, width2,
      oldsTemp3,
      &surfaceTemp,
      nlyrs2, fc2, wp2,
      sTconst, deltaX, theMaxDepth,
      nRgr, &ptr_stError, SW_All.SoilWat.lyrFrozen
    );

    // Test surface temp equals surface_temperature_under_snow() because snow > 0
    snowdepth = 5;

    for (k = 0; k < nlyrs2; k++) {
      sTemp3[k] = oldsTemp3[k];
    }

    soil_temperature(&surface_max, &surface_min, SW_All.SoilWat.lyrFrozen,
      airTemp, pet, aet, biomass, swc2, swc_sat2, bDensity2, width2,
      sTemp3, &surfaceTemp, nlyrs2, bmLimiter, t1Param1, t1Param2,
      t1Param3, csParam1, csParam2, shParam, snowdepth, sTconst, deltaX, theMaxDepth,
      nRgr, snow, max_air_temp, min_air_temp, H_gt, SW_All.Model.year,
      SW_All.Model.doy, min_temp, max_temp, &ptr_stError);

    EXPECT_EQ(surfaceTemp, surface_temperature_under_snow(airTemp, snow));
    EXPECT_NE(surfaceTemp, airTemp + ((t1Param2 * (biomass - bmLimiter)) / t1Param3));
    EXPECT_NE(surfaceTemp, airTemp + (t1Param1 * pet * (1. - (aet / pet)) * (1. - (biomass / bmLimiter))));

    //Test surface temp equals equatin when biomass < blimititer & snow = 0
    snowdepth = 0;
    biomass = 100;
    for (k = 0; k < nlyrs2; k++) {
      sTemp3[k] = oldsTemp3[k];
    }

    soil_temperature(&surface_max, &surface_min, SW_All.SoilWat.lyrFrozen,
      airTemp, pet, aet, biomass, swc2, swc_sat2, bDensity2, width2,
      sTemp3, &surfaceTemp, nlyrs2, bmLimiter, t1Param1, t1Param2,
      t1Param3, csParam1, csParam2, shParam, snowdepth, sTconst, deltaX, theMaxDepth,
      nRgr, snow, max_air_temp, min_air_temp, H_gt, SW_All.Model.year,
      SW_All.Model.doy, min_temp, max_temp, &ptr_stError);

    EXPECT_EQ(surfaceTemp, airTemp + (t1Param1 * pet * (1. - (aet / pet)) * (1. - (biomass / bmLimiter))));
    EXPECT_NE(surfaceTemp, airTemp + ((t1Param2 * (biomass - bmLimiter)) / t1Param3));
    EXPECT_NE(surfaceTemp, surface_temperature_under_snow(airTemp, snow));

    //Test surface temp equals equation when biomass < blimititer & snow = 0
    biomass = 305;
    for (k = 0; k < nlyrs2; k++) {
      sTemp3[k] = oldsTemp3[k];
    }

    soil_temperature(&surface_max, &surface_min, SW_All.SoilWat.lyrFrozen,
      airTemp, pet, aet, biomass, swc2, swc_sat2, bDensity2, width2,
      sTemp3, &surfaceTemp, nlyrs2, bmLimiter, t1Param1, t1Param2,
      t1Param3, csParam1, csParam2, shParam, snowdepth, sTconst, deltaX, theMaxDepth,
      nRgr, snow, max_air_temp, min_air_temp, H_gt, SW_All.Model.year,
      SW_All.Model.doy, min_temp, max_temp, &ptr_stError);

    EXPECT_EQ(surfaceTemp, airTemp + ((t1Param2 * (biomass - bmLimiter)) / t1Param3));
    EXPECT_NE(surfaceTemp, airTemp + (t1Param1 * pet * (1. - (aet / pet)) * (1. - (biomass / bmLimiter))));
    EXPECT_NE(surfaceTemp, surface_temperature_under_snow(airTemp, snow));

    // checks for  lyrTemp_to_lyrSoil_temperature
    int resultValue2 = sizeof(sTemp3) / sizeof(sTemp3[0]);
    EXPECT_EQ(MAX_LAYERS, resultValue2); // when the number of soil layers is MAX_LAYERS, length of sTemp3 should be MAX_LAYERS

    for (k = 0; k < nlyrs2; k++)
    {
      //swprintf("\n k %u, sTemp3 %f", k, sTemp3[k]);
      EXPECT_GT(sTemp3[k], -100); // Sense check
      EXPECT_LT(sTemp3[k], 100); // Sense check&
    }

    // Expect that oldsTempR is updated to sTempR for the next day
    for (k = 0; k <= nRgr + 1; k++)
    {
      //swprintf("\n k %u, newoldtempR %f", k, stValues.oldavgLyrTempR[k]);
      EXPECT_NE(stValues.oldavgLyrTempR[k], SW_MISSING);
    }

    double *array_list[] = { swc2, swc_sat2, fc2, wp2, min_temp, max_temp};
    for (i = 0; i < length(array_list); i++){
      delete[] array_list[i];
    }

    // Reset to global state
    Reset_SOILWAT2_after_UnitTest();
  }

  // Test that main soil temperature functions fails when it is supposed to
  TEST(SWFlowTempDeathTest, MainSoilTemperatureFunction) {

    unsigned int nlyrs = 1, nRgr = 65;
    double airTemp = 25.0, pet = 5.0, aet = 4.0, biomass = 100., surfaceTemp = 15.,
    bmLimiter = 300., t1Param1 = 15., t1Param2 = -4., t1Param3 = 600., csParam1 =0.00070,
    csParam2 = 0.00030, shParam = 0.18, snowdepth = 5, sTconst = 4.15, deltaX = 15,
    theMaxDepth = 990., snow = 1, max_air_temp = 10.1, min_air_temp = -5.0, H_gt = 300.0,
    surface_max = 10.6, surface_min = -6.8;
    Bool ptr_stError = swFALSE;

    double swc[] = {1.0}, swc_sat[] = {1.5}, bDensity[] = {1.8}, width[] = {20},
      sTemp[1], min_temp[] = {10.0}, max_temp[] = {1.0};

    // Should fail when soil_temperature was not initialized
    EXPECT_DEATH_IF_SUPPORTED(
      soil_temperature(
        &surface_max, &surface_min, SW_All.SoilWat.lyrFrozen,
        airTemp, pet, aet, biomass, swc, swc_sat, bDensity, width,
        sTemp, &surfaceTemp, nlyrs, bmLimiter, t1Param1, t1Param2,
        t1Param3, csParam1, csParam2, shParam, snowdepth,
        sTconst, deltaX, theMaxDepth, nRgr, snow, max_air_temp,
        min_air_temp, H_gt, SW_All.Model.year, SW_All.Model.doy,
        min_temp, max_temp, &ptr_stError
      ),
      "SOILWAT2 ERROR soil temperature module was not initialized"
    );

    //Reset to global state
    Reset_SOILWAT2_after_UnitTest();
  }
}
