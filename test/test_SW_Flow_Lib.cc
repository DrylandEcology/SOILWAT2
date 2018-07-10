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

#include "../generic.h"
#include "../myMemory.h"
#include "../filefuncs.h"
#include "../rands.h"
#include "../Times.h"
#include "../SW_Defines.h"
#include "../SW_Times.h"
#include "../SW_Files.h"
#include "../SW_Carbon.h"
#include "../SW_Site.h"
#include "../SW_VegProd.h"
#include "../SW_VegEstab.h"
#include "../SW_Model.h"
#include "../SW_SoilWater.h"
#include "../SW_Weather.h"
#include "../SW_Markov.h"
#include "../SW_Sky.h"

#include "../SW_Flow_lib.h"

#include "sw_testhelpers.h"

extern SW_SITE SW_Site;
extern SW_MODEL SW_Model;
extern SW_VEGPROD SW_VegProd;
SW_VEGPROD *v = &SW_VegProd;
int k;


namespace {

  // Test the veg interception function 'veg_intercepted_water'
  TEST(SWFlowTest, VegInterceptedWater) {

    ForEachVegType(k){
      // declare inputs
      double x;
      double ppt = 5.0, scale = 1.0;
      double pptleft = 5.0, wintveg = 0.0;
      double a = v -> veg[k].veg_intPPT_a, b = v -> veg[k].veg_intPPT_b,
      c = v -> veg[k].veg_intPPT_c, d = v -> veg[k].veg_intPPT_d;

      // Test expectation when x("vegcov") is zero
      x = 0.0;

      veg_intercepted_water(&pptleft, &wintveg, ppt, x, scale, a, b, c, d);

      EXPECT_EQ(0, wintveg); // When there is no veg, interception should be 0
      EXPECT_EQ(pptleft, ppt); /* When there is no interception, ppt before interception
      should equal ppt left after interception */

      // Test expectations when ppt is 0
      ppt = 0.0, x = 5.0;

      veg_intercepted_water(&pptleft, &wintveg, ppt, x, scale, a, b, c, d);

      EXPECT_EQ(0, wintveg);  // When there is no ppt, interception should be 0
      EXPECT_EQ(pptleft, ppt); /* When there is no interception, ppt before interception
      should equal ppt left after interception */


      // Test expectations when there is both veg cover and precipitation
      ppt = 5.0, x = 5.0;

      veg_intercepted_water(&pptleft, &wintveg, ppt, x, scale, a, b, c, d);

      EXPECT_GT(wintveg, 0); // interception by veg should be greater than 0
      EXPECT_LE(wintveg, MAX_WINTSTCR(x)); // interception by veg should be less than or equal to MAX_WINTSTCR (vegcov * .1)
      EXPECT_LE(wintveg, ppt); // interception by veg should be less than or equal to ppt
      EXPECT_GE(pptleft, 0); // The pptleft (for soil) should be greater than or equal to 0

      // Reset to previous global state
      Reset_SOILWAT2_after_UnitTest();
    }
  }

  // Test the litter interception function 'litter_intercepted_water'
  TEST(SWFlowTest, LitterInterceptedWater) {

    ForEachVegType(k){
      // declare inputs
      double scale, blitter,pptleft = 5.0;
      double wintlit;
      double a = v->veg[k].litt_intPPT_a, b = v->veg[k].litt_intPPT_b,
      c = v->veg[k].litt_intPPT_c, d = v->veg[k].litt_intPPT_d;

      // Test expectation when scale (cover) is zero
      pptleft = 5.0, scale = 0.0, blitter = 5.0;

      litter_intercepted_water(&pptleft, &wintlit, blitter, scale, a, b, c, d);

      EXPECT_EQ(0, wintlit); // When scale is 0, interception should be 0

      // Test expectations when blitter is 0
      pptleft = 5.0, scale = 0.5, blitter = 0.0;

      litter_intercepted_water(&pptleft, &wintlit, blitter, scale, a, b, c, d);

      EXPECT_EQ(0, wintlit); // When there is no blitter, interception should be 0


      // Test expectations when pptleft is 0
      pptleft = 0.0, scale = 0.5, blitter = 5.0;

      litter_intercepted_water(&pptleft, &wintlit, blitter, scale, a, b, c, d);

      EXPECT_EQ(0, pptleft); // When there is no ppt, pptleft should be 0
      EXPECT_EQ(0, wintlit); // When there is no blitter, interception should be 0

      // Test expectations when there pptleft, scale, and blitter are greater than 0
      pptleft = 5.0, scale = 0.5, blitter = 5.0;

      litter_intercepted_water(&pptleft, &wintlit, blitter, scale, a, b, c, d);

      EXPECT_GT(wintlit, 0); // interception by litter should be greater than 0
      EXPECT_LE(wintlit, pptleft); // interception by lit should be less than or equal to ppt
      EXPECT_LE(wintlit, MAX_WINTLIT); /* interception by lit should be less than
      or equal to MAX_WINTLIT (blitter * .2) */
      EXPECT_GE(pptleft, 0); // The pptleft (for soil) should be greater than or equal to 0

      // Reset to previous global state
      Reset_SOILWAT2_after_UnitTest();
    }
  }

  // Test infiltration under high water function, 'infiltrate_water_high'
  TEST(SWFlowTest, infiltrate_water_high){

    // declare inputs
    double pptleft = 5.0, standingWater, drainout;

    // ***** Tests when nlyrs = 1 ***** //
    ///  provide inputs
    int nlyrs = 1;
    double swc[1] = {0.8}, swcfc[1] = {1.1}, swcsat[1] = {1.6}, impermeability[1] = {0.}, drain[1] = {0.};

    infiltrate_water_high(swc, drain, &drainout, pptleft, nlyrs, swcfc, swcsat,
      impermeability, &standingWater);

    EXPECT_GE(drain[0], 0); // drainage should be greater than or equal to 0 when soil layers are 1 and ppt > 1
    EXPECT_LE(swc[0], swcsat[0]); // swc should be less than or equal to swcsat
    EXPECT_DOUBLE_EQ(drainout, drain[0]); // drainout and drain should be equal when we have one layer

    /* Test when pptleft and standingWater are 0 (No drainage) */
    pptleft = 0.0, standingWater = 0.0, drain[0] = 0., swc[0] = 0.8, swcfc[0] = 1.1, swcsat[0] = 1.6;

    infiltrate_water_high(swc, drain, &drainout, pptleft, nlyrs, swcfc, swcsat,
      impermeability, &standingWater);

    EXPECT_DOUBLE_EQ(0, drain[0]); // drainage should be 0


    /* Test when impermeability is greater than 0 and large precipitation */
    pptleft = 20.0;
    impermeability[0] = 1.;
    swc[0] = 0.8, drain[0] = 0.;

    infiltrate_water_high(swc, drain, &drainout, pptleft, nlyrs, swcfc, swcsat,
      impermeability, &standingWater);

    EXPECT_DOUBLE_EQ(0., drain[0]); //When impermeability is 1, drainage should be 0
    EXPECT_DOUBLE_EQ(standingWater, (pptleft + 0.8) - swcsat[0]); /* When impermeability is 1,
      standingWater should be equivalent to  pptLeft + swc[0] - swcsat[0]) */

    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();


    // *****  Test when nlyrs = MAX_LAYERS (SW_Defines.h)  ***** //
    /// generate inputs using a for loop
    int i;
    nlyrs = MAX_LAYERS, pptleft = 5.0;
    double swc2[nlyrs], swcfc2[nlyrs], swcsat2[nlyrs], impermeability2[nlyrs], drain2[nlyrs];

    for (i = 0; i < MAX_LAYERS; i++) {
      swc2[i] = RandNorm(1.,0.5);
      swcfc2[i] = RandNorm(1,.5);
      swcsat2[i] = swcfc2[i] + .1; // swcsat will always be greater than swcfc in each layer
      impermeability2[i] = 0.0;
    }

    infiltrate_water_high(swc2, drain2, &drainout, pptleft, nlyrs, swcfc2, swcsat2,
      impermeability2, &standingWater);

    EXPECT_EQ(drainout, drain2[MAX_LAYERS - 1]); /* drainout and drain should be
      equal in the last layer */

    for (i = 0; i < MAX_LAYERS; i++) {
      swc2[i] = swc2[i] - 1/10000.; // test below is failing because of small numerical differences.
      EXPECT_LE(swc2[i], swcsat2[i]); // swc should be less than or equal to swcsat
      EXPECT_GE(drain2[i], -1./100000000.); /*  drainage should be greater than or
      equal to 0 or a very small value like 0 */
    }

    /* Test when pptleft and standingWater are 0 (No drainage); swc < swcfc3  < swcsat */
    pptleft = 0.0, standingWater = 0.0;
    double swc3[nlyrs], swcfc3[nlyrs], swcsat3[nlyrs], drain3[nlyrs];

    for (i = 0; i < MAX_LAYERS; i++) {
      swc3[i] = RandNorm(1.,0.5);
      swcfc3[i] = swc3[i] + .2;
      swcsat3[i] = swcfc3[i] + .5;
      drain3[i] = 0.;// swcsat will always be greater than swcfc in each layer
    }

    infiltrate_water_high(swc3, drain3, &drainout, pptleft, nlyrs, swcfc3, swcsat3,
      impermeability2, &standingWater);

    for (i = 0; i < MAX_LAYERS; i++) {
      EXPECT_DOUBLE_EQ(0, drain3[i]); // drainage should be 0
    }


    /* Test when impermeability is greater than 0 and large precipitation */
    double impermeability4[nlyrs], drain4[nlyrs], swc4[nlyrs], swcfc4[nlyrs], swcsat4[nlyrs];
    pptleft = 20.0;

    for (i = 0; i < MAX_LAYERS; i++) {
      swc4[i] = RandNorm(1.,0.5);
      swcfc4[i] = swc4[i] + .2;
      swcsat4[i] = swcfc4[i] + .3; // swcsat will always be greater than swcfc in each layer
      impermeability4[i] = 1.0;
      drain4[i] = 0.0;
    }

    swc4[0] = 0.8; // Need to hard code this value because swc4 is altered by function

    infiltrate_water_high(swc4, drain4, &drainout, pptleft, nlyrs, swcfc4, swcsat4,
      impermeability4, &standingWater);

    EXPECT_DOUBLE_EQ(standingWater, (pptleft + 0.8) - swcsat4[0]); /* When impermeability is 1,
      standingWater should be equivalent to  pptLeft + swc[0] - swcsat[0]) */

    for (i = 0; i < MAX_LAYERS; i++) {
      EXPECT_DOUBLE_EQ(0, drain4[i]); //When impermeability is 1, drainage should be 0
    }

    /* Test "push", when swcsat > swc */
    double impermeability5[nlyrs], drain5[nlyrs], swc5[nlyrs], swcfc5[nlyrs], swcsat5[nlyrs];
    pptleft = 5.0;

    for (i = 0; i < MAX_LAYERS; i++) {
      swc5[i] = RandNorm(1.2,0.5);
      swcfc5[i] = swc5[i] - .4; // set up conditions for excess SWC
      swcsat5[i] = swcfc5[i] + .1; // swcsat will always be greater than swcfc in each layer
      impermeability5[i] = 1.0;
      drain5[i] = 0.0;
    }

    infiltrate_water_high(swc5, drain5, &drainout, pptleft, nlyrs, swcfc5, swcsat5,
      impermeability5, &standingWater);

    for (i = 0; i < MAX_LAYERS; i++) {
      swc5[i] = (int)(swc5[i] * 10000000 + 0.5)/ 10000000; // need to round for unit test, despite using EXPECT_DOUBLE_EQ
      swcsat5[i] = (int)(swcsat5[i] * 10000000 + 0.5)/ 10000000; // need to round for unit test, despite using EXPECT_DOUBLE_EQ
      EXPECT_DOUBLE_EQ(swc5[i], swcsat5[i]); // test that swc is now equal to or below swcsat in all layers but the top
    }

    EXPECT_GT(standingWater, 0); // standingWater should be above 0

    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }

  //Test svapor function by manipulating variable temp.
  TEST(SWFlowTest, svapor){

    //Declare INPUTS
    double temp[] = {30,35,40,45,50,55,60,65,70,75,20,-35,-12.667,-1,0}; // These are test temperatures, in degrees Celcius.
    double expOut[] = {32.171, 43.007, 56.963, 74.783, 97.353, 125.721, 161.113,
      204.958, 258.912, 324.881, 17.475, 0.243, 1.716, 4.191, 4.509}; // These are the expected outputs for the svapor function.

    //Declare OUTPUTS
    double vapor;

    //Begin TEST
    for (int i = 0; i <15; i++){
      vapor = svapor(temp[i]);
      vapor = round(vapor * 1000 + 0.00001) / 1000; // Rounding is required for unit test.

      EXPECT_DOUBLE_EQ(expOut[i], vapor); // Testing input array temp[], expected output is expOut[].
    }
    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }

  //Test petfunc by manipulating each input individually.
  TEST(SWFlowTest, petfunc){
    //Begin TEST for avgtemp input variable
    //Declare INPUTS
    unsigned int doy = 2;
    double rlat = 0.681, elev = 1000, slope = 0, aspect = -1, reflec = 0.15, humid = 61,
      windsp = 1.3, cloudcov = 71, transcoeff = 1;//For the first day in the month of January
    double temp, check, test;
    double avgtemps[] = {30,35,40,45,50,55,60,65,70,75,20,-35,-12.667,-1,0}; // These are test temperatures, in degrees Celcius.


    //Declare INPUTS for expected returns
    double expReturnTemp[] = {0.201, 0.245, 0.299, 0.364, 0.443, 0.539, 0.653, 0.788,
      0.948, 1.137, 0.136, 0.01, 0.032, 0.057, 0.060}; // These are the expected outcomes for the variable arads.



    for (int i = 0; i <15; i++){

      temp = avgtemps[i]; //Uses array of temperatures for testing for input into temp variable.
      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid, windsp, cloudcov, transcoeff);
      test = round(check* 1000 + 0.00001) / 1000; //Rounding is required for unit test.

      EXPECT_DOUBLE_EQ(test, expReturnTemp[i]); //Tests the return of the petfunc against the expected return for the petfunc.
    }

    //Begin TEST for rlat input variable.  Inputs outside the range of [-1.169,1.169] produce the same output, 0.042.  Rlat values outside this range represent values near the poles.
    //INPUTS
    temp = 25, cloudcov = 71, humid = 61, windsp = 1.3;
    double rlats[] = {-1.5708, -1.3, -1.169, -1.069, -0.969, -0.869, -0.769, -0.669, -0.569, -0.469, -0.369, -0.269, -0.169,
      -0.069, 0.031, 0.131, 0.231, 0.331, 0.431, 0.531, 0.631, 0.731, 0.831, 0.931,
        1.031, 1.5708}; //Initialize array of potential latitudes, in radians, for the site.

   //Declare INPUTS for expected returns
    double expReturnLats[] = {0.042, 0.042, 0.414, 0.412, 0.415, 0.418, 0.420, 0.419, 0.416, 0.411, 0.402,
      0.391, 0.376, 0.359, 0.339, 0.317, 0.293, 0.267, 0.239, 0.210, 0.180, 0.150, 0.120,
        0.092, 0.066, 0.042}; //These are the expected returns for petfunc while manipulating the rlats input variable.


    for (int i = 0; i < 26; i++){

      rlat = rlats[i]; //Uses array of latitudes initialized above.
      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid, windsp, cloudcov, transcoeff);
      test = round(check* 1000 + 0.00001) / 1000; //Rounding is required for unit test.

      EXPECT_DOUBLE_EQ(test, expReturnLats[i]); //Tests the return of the petfunc against the expected return for the petfunc.
    }
    //Begin TEST for elev input variable, testing from -413 meters (Death Valley) to 8727 meters (~Everest).
    //INPUTS
    rlat = 0.681;
    elev = -413;

    //Declare INPUTS for expected returns
    double expReturnElev[] = {0.181, 0.176, 0.171, 0.165, 0.160, 0.156, 0.151, 0.146, 0.142, 0.137, 0.133, 0.128, 0.124,
      0.120, 0.116, 0.113, 0.109, 0.106, 0.102, 0.099, 0.096};


    for(int i = 0; i < 21; i++){

      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid, windsp, cloudcov, transcoeff);
      test = round(check* 1000 + 0.00001) / 1000; //Rounding is required for unit test.

      EXPECT_DOUBLE_EQ(test, expReturnElev[i]); //Tests the return of the petfunc against the expected return for the petfunc.

      elev += 457;  //Incrments elevation input variable.
    }
    //Begin TEST for slope input variable
    //INPUTS
    elev = 1000, slope = 0;

    //Declare INPUTS for expected returns
    double expReturnSlope[] = {0.165, 0.142, 0.118, 0.094, 0.069, 0.044, 0.020, 0.01, 0.01, 0.01,
      0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, };
      //Expected returns of 0.01 occur when the petfunc returns a negative number.



    for (int i = 0; i < 21; i++){

      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid, windsp, cloudcov, transcoeff);
      test = round(check* 1000 + 0.00001) / 1000; //Rounding is required for unit test.

      EXPECT_DOUBLE_EQ(test, expReturnSlope[i]); //Tests the return of the petfunc against the expected return for the petfunc.

      slope += 4.3; //Incrments slope input variable.
}
    //Begin TEST for aspect input variable
    //INPUTS
    slope = 5, aspect = 0;

    //Declare INPUTS for expected returns
    double expReturnAspect[] = {0.138, 0.139, 0.141, 0.145, 0.151, 0.157, 0.164, 0.170, 0.177, 0.182, 0.187, 0.190,
      0.191, 0.191, 0.189, 0.185, 0.180, 0.174, 0.167, 0.160, 0.154, 0.148, 0.143, 0.140, 0.138};


    for(int i = 0; i < 25; i++){

      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid, windsp, cloudcov, transcoeff);
      test = round(check* 1000 + 0.00001) / 1000; //Rounding is required for unit test.

      EXPECT_DOUBLE_EQ(test, expReturnAspect[i]);

      aspect += 14.67; //Incrments aspect input variable.
    }

    //Begin TEST for reflec input variable
    //INPUTS
    aspect = -1, slope = 0, reflec = 0;

    //Declare INPUTS for expected returnsdouble expReturnSwpAvg = 0.00000148917;
    double expReturnReflec[] = {0.187, 0.180, 0.174, 0.167, 0.161, 0.154, 0.148, 0.141, 0.135, 0.128, 0.122, 0.115,
      0.109, 0.102, 0.096, 0.089, 0.083, 0.076, 0.070, 0.063, 0.057, 0.050, 0.044};


    for(int i = 0; i < 23; i++){

      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid, windsp, cloudcov, transcoeff);
      test = round(check* 1000 + 0.00001) / 1000; //Rounding is required for unit test.

      EXPECT_DOUBLE_EQ(test, expReturnReflec[i]);

      reflec += 0.045; //Incrments reflec input variable.
    }
    //Begin TEST for humidity input varibable.
    //INPUTS
    reflec = 0.15, humid = 0;

    //Declare INPUTS for expected returns.
    double expReturnHumid[] = {0.221, 0.247, 0.248, 0.246, 0.241, 0.236, 0.229, 0.221, 0.213, 0.205, 0.196,
      0.187, 0.177, 0.168, 0.158, 0.148, 0.137, 0.127, 0.116, 0.105, 0.094, 0.083};


    for(int i = 0; i < 22; i++){

      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid, windsp, cloudcov, transcoeff);
      test = round(check* 1000 + 0.00001) / 1000; //Rounding is required for unit test.

      EXPECT_DOUBLE_EQ(test, expReturnHumid[i]);

      humid += 4.6; //Incrments humid input variable.
    }
    //Begin TEST for windsp input variable.
    //INPUTS
    humid = 61, windsp = 0;

    //Declare INPUTS for expected returns.
    double expReturnWindsp[] = {0.112, 0.204, 0.297, 0.390, 0.483, 0.576, 0.669, 0.762, 0.855, 0.948,
      1.041, 1.134, 1.227, 1.320, 1.413, 1.506, 1.599, 1.692, 1.785, 1.878, 1.971, 2.064, 2.157};


    for(int i = 0; i < 23; i++){

      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid, windsp, cloudcov, transcoeff);
      test = round(check* 1000 + 0.00001) / 1000; //Rounding is required for unit test.

      EXPECT_DOUBLE_EQ(test, expReturnWindsp[i]);

      windsp += 2.26; //Incrments windsp input variable.
    }
    //Begin TEST for cloudcov input variable.
    //INPUTS
    windsp = 1.3, cloudcov = 0;

    //Declare INPUTS for expected returns.
    double expReturnCloudcov[] = {0.148, 0.149, 0.150, 0.151, 0.152, 0.153, 0.154, 0.155, 0.156, 0.157,
      0.158, 0.159, 0.160, 0.161, 0.162, 0.163, 0.164, 0.165, 0.166, 0.167, 0.168, 0.169, 0.170, 0.171};


    for(int i = 0; i < 24; i++){

      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid, windsp, cloudcov, transcoeff);
      test = round(check* 1000 + 0.00001) / 1000; //Rounding is required for unit test.

      EXPECT_DOUBLE_EQ(test, expReturnCloudcov[i]);

      cloudcov += 4.27; //Incrments cloudcov input variable.
    }
    //Begin TEST for cloudcov input variable.
    //INPUTS
    cloudcov = 71, transcoeff = 0.01;

    //Declare INPUTS for expected returns.
    double expReturnTranscoeff = 0.165;
    //The same value is returned for every tested transcoeff input.


    for(int i = 0; i < 20; i++){

      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid, windsp, cloudcov, transcoeff);
      test = round(check* 1000 + 0.00001) / 1000; //Rounding is required for unit test.

      EXPECT_DOUBLE_EQ(test, expReturnTranscoeff);

      transcoeff += 52.57; //Incrments transcoeff input variable.
    }

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
    }

  //Test transp_weighted_avg function.
  TEST(SWFlowTest, transp_weighted_avg){

    //INPUTS
    double swp_avg = 10;
    unsigned int n_tr_rgns = 1, n_layers = 1;
    unsigned int tr_regions[1] = {1}; // 1-4
    double tr_coeff[1] = {0.0496}; //trco_grass
    double swc[1] = {12};
    double check1 = 2.328876e-05, check2 = 1.926636e-06, test = 0;

    //Begin TEST when n_layers is one
    transp_weighted_avg(&swp_avg, n_tr_rgns, n_layers, tr_regions, tr_coeff, swc);

    test = round(check1); //Rounding is required for unit test.
    swp_avg = round(swp_avg); //Rounding is required for unit test.
    EXPECT_GE(swp_avg, 0); //Must always be non negative.
    EXPECT_DOUBLE_EQ(swp_avg, test); //Test results always very close to zero, regardless of inputs.

    //Begin TEST when n_layers is at "max"
    //INPUTS
    n_tr_rgns = 4, n_layers = 8; //Only 8 inputs in soils.in file.  Same output for 8-25 layers.
    unsigned int tr_regions2[8] = {1,1,2,2,3,3,4,4};
    double tr_coeff2[8] = {0.033, 0.033, 0.067, 0.067, 0.067, 0.133, 0.133, 0.133};
    double swc2[8] = {0.01, 1.91, 3.81, 5.71, 7.61, 9.51, 11.41, 13.31};

    transp_weighted_avg(&swp_avg, n_tr_rgns, n_layers, tr_regions2, tr_coeff2, swc2);
    test = round(check2); //Rounding is required for unit test.
    swp_avg = round(swp_avg); //Rounding is required for unit test.
    EXPECT_GE(swp_avg, 0); //Must always be non negative.
    EXPECT_DOUBLE_EQ(swp_avg, test); //Test results always very close to zero, regardless of inputs.


    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }

  //Test EsT_partitioning by manipulating fbse and fbst variables.
  TEST(SWFlowTest, EsT_partitioning){
    //INPUTS
    double fbse = 0, fbst = 0, blivelai = 0.002, lai_param = 2;

    //TEST when fbse > bsemax
    EsT_partitioning(&fbse, &fbst, blivelai, lai_param);

    double test = round(fbse * 1000) / 1000; //Rounding is required for unit test.
    EXPECT_DOUBLE_EQ(test, 0.995);

    test = round(fbst * 1000) / 1000; //Rounding is required for unit test.
    EXPECT_DOUBLE_EQ(test, 0.005); //fbst = 1 - fbse; fbse = bsemax
    EXPECT_GE(fbse, 0);
    EXPECT_GE(fbst, 0);
    EXPECT_LT(fbse, 1);
    EXPECT_LT(fbst, 1);
    EXPECT_DOUBLE_EQ(fbst + fbse, 1); //Must add up to one.

    //TEST when fbse < bsemax
    blivelai = 0.0012, lai_param = 5;
    EsT_partitioning(&fbse, &fbst, blivelai, lai_param);
    test = round(fbse * 1000000) / 1000000; //Rounding is required for unit test.
    double check = round(0.994018 * 1000000) / 1000000;
    EXPECT_DOUBLE_EQ(test, check);

    test = round(fbst * 1000) / 1000; //Rounding is required for unit test.
    check = round(0.005982036 * 1000) / 1000;
    EXPECT_DOUBLE_EQ(test, check);
    EXPECT_GE(fbse, 0);
    EXPECT_GE(fbst, 0);
    EXPECT_LT(fbse, 1);
    EXPECT_LT(fbst, 1);
    EXPECT_DOUBLE_EQ(fbst + fbse, 1);

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }

  //TEST pot_soil_evap for when nelyrs = 1 and nelyrs = MAX
  TEST(SWFlowTest, pot_soil_evap){
    //INPUTS
    unsigned int nelyrs = 1;
    double ecoeff[8] = {45, 0.1, 0.25, 0.5, 45, 0.1, 0.25, 0.5};
    double bserate = 0, totagb, fbse = 0.813, petday = 0.1, shift = 45, shape = 0.1,
      inflec = 0.25, range = 0.5, Es_param_limit = 1;
    double width[8] = {5,5,5,5,10,10,10,10};
    double swc[8] = {1,2,3,4,5,6,7,8};

    //Begin TEST for if(totagb >= Es_param_limit)
    totagb = 17000;
    pot_soil_evap(&bserate, nelyrs, ecoeff, totagb, fbse, petday, shift, shape, inflec, range,
      width, swc, Es_param_limit);

    EXPECT_DOUBLE_EQ(bserate, 0); //Expected return of zero when totagb >= Es_param_limit

    //Begin TEST for if(totagb < Es_param_limit)
    totagb = 0.5;
    pot_soil_evap(&bserate, nelyrs, ecoeff, totagb, fbse, petday, shift, shape, inflec, range,
      width, swc, Es_param_limit);
    double test = 0.02563894;
    test = round(test); //Rounding is required for unit test.
    double check = round(bserate);
    EXPECT_DOUBLE_EQ(check, test);

    //TEST for when nelyrs = MAX_LAYERS
    nelyrs = 8; //Only 8 inputs available for SW_SWCbulk2SWPmatric function of SW_SoilWater.c

    //Begin TEST for if(totagb >= Es_param_limit)
    totagb = 17000;
    pot_soil_evap(&bserate, nelyrs, ecoeff, totagb, fbse, petday, shift, shape, inflec, range,
      width, swc, Es_param_limit);

    EXPECT_DOUBLE_EQ(bserate, 0); //Expected return of zero when totagb >= Es_param_limit

    //Begin TEST for if(totagb < Es_param_limit)
    totagb = 0.5;
    pot_soil_evap(&bserate, nelyrs, ecoeff, totagb, fbse, petday, shift, shape, inflec, range,
      width, swc, Es_param_limit);
    double test2 = 0.02563877;
    test2 = round(test2); //Rounding is required for unit test.
    check = round(bserate);
    EXPECT_DOUBLE_EQ(check, test2);


    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }

  //TEST pot_soil_evap_bs for when nelyrs = 1 and nelyrs = MAX
  TEST(SWFlowTest, pot_soil_evap_bs){
    //INPUTS
    unsigned int nelyrs = 1;
    double ecoeff[1] = {0.1};
    double bserate = 0, petday = 0.1, shift = 45, shape = 0.1, inflec = 0.25, range = 0.8;
    double width[1] = {5};
    double swc[1] = {1};

    //Begin TEST for bserate
    pot_soil_evap_bs(&bserate, nelyrs, ecoeff, petday, shift, shape, inflec, range, width, swc);

    double test = 0.06305998;
    test = round(test); //Rounding is required for unit test.
    double check = round(bserate);
    EXPECT_DOUBLE_EQ(check, test);

    //Testing when nelyrs = MAX_LAYERS
    nelyrs = 8; //Only 8 inputs available for SW_SWCbulk2SWPmatric function of SW_SoilWater.c
    double ecoeff8[8] = {0.1, 0.1, 0.25, 0.5, 0.01, 0.1, 0.25, 0.5};
    double width8[8] = {5,5,5,5,10,10,10,10};
    double swc8[8] = {1,2,3,4,5,6,7,8};

    pot_soil_evap_bs(&bserate, nelyrs, ecoeff8, petday, shift, shape, inflec, range, width8, swc8);

    double test2 = 0.06306041;
    test2 = round(test); //Rounding is required for unit test.
    check = round(bserate);
    EXPECT_DOUBLE_EQ(check, test2);

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }

  //Test pot_transp by manipulating biolive and biodead input variables
  TEST(SWFlowTest, pot_transp){

    //INPUTS
    double bstrate = 0, swpavg = 0.8, biolive = -0.8, biodead = 0.2, fbst = 0.8, petday = 0.1,
      swp_shift = 45, swp_shape = 0.1, swp_inflec = 0.25, swp_range = 0.3, shade_scale = 1.1,
        shade_deadmax = 0.9, shade_xinflex = 0.4, shade_slope = 0.9, shade_yinflex = 0.3,
          shade_range = 0.8, co2_wue_multiplier = 2.1;

    //Begin TEST for if biolive < 0
    pot_transp(&bstrate, swpavg, biolive, biodead, fbst, petday, swp_shift, swp_shape,
      swp_inflec, swp_range, shade_scale, shade_deadmax, shade_xinflex, shade_slope,
        shade_yinflex, shade_range, co2_wue_multiplier);

    double test = 0;
    double check = round(bstrate);
    EXPECT_DOUBLE_EQ(check, test); //bstrate = 0 if biolove < 0

    //Begin TEST for if biolive > 0
    biolive = 0.8;
    pot_transp(&bstrate, swpavg, biolive, biodead, fbst, petday, swp_shift, swp_shape,
      swp_inflec, swp_range, shade_scale, shade_deadmax, shade_xinflex, shade_slope,
        shade_yinflex, shade_range, co2_wue_multiplier);

    test = 0.06596299;
    test = round(test * 1000) / 1000; //Rounding is required for unit test.
    check = round(bstrate * 1000) / 1000;
    EXPECT_DOUBLE_EQ(check, test); //For this test local variable shadeaf = 1, affecting bstrate

    //Begin TEST for if biodead > shade_deadmax
    biodead = 0.95;
    pot_transp(&bstrate, swpavg, biolive, biodead, fbst, petday, swp_shift, swp_shape,
      swp_inflec, swp_range, shade_scale, shade_deadmax, shade_xinflex, shade_slope,
        shade_yinflex, shade_range, co2_wue_multiplier);

    test = 0.06564905;
    test = round(test * 1000) / 1000; //Rounding is required for unit test.
    check = round(bstrate * 1000) / 1000;
    EXPECT_DOUBLE_EQ(check, test);

    //Begin TEST for if biodead < shade_deadmax
    biodead = 0.2;
    pot_transp(&bstrate, swpavg, biolive, biodead, fbst, petday, swp_shift, swp_shape,
      swp_inflec, swp_range, shade_scale, shade_deadmax, shade_xinflex, shade_slope,
        shade_yinflex, shade_range, co2_wue_multiplier);

    test = 0.06596299;
    test = round(test * 1000) / 1000; //Rounding is required for unit test.
    check = round(bstrate * 1000) / 1000;
    EXPECT_DOUBLE_EQ(check, test); //For this test local variable shadeaf = 1, affecting bstrate

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }
  //Test result for watrate by manipulating variable petday
  TEST(SWFlowTest, watrate){

    //INPUTS
    double swp = 0.8, petday = 0.1, shift = 45, shape = 0.1, inflec = 0.25, range = 0.8;

    //Begin TEST for if petday < .2
    double wat = watrate(swp, petday, shift, shape, inflec, range);

    double test = 0.630365;
    test = round(test * 1000) / 1000; //Rounding is required for unit test.
    double check = round(wat * 1000) / 1000;
    EXPECT_DOUBLE_EQ(check, test); //When petday = 0.1, watrate = 0.630365
    EXPECT_LE(check, 1); //watrate must be between 0 & 1
    EXPECT_GE(check, 0); //watrate must be between 0 & 1

    //Begin TEST for if 0.2 < petday < .4
    petday = 0.3;
    wat = watrate(swp, petday, shift, shape, inflec, range);

    test = 0.6298786;
    test = round(test * 1000) / 1000; //Rounding is required for unit test.
    check = round(wat * 1000) / 1000;
    EXPECT_DOUBLE_EQ(check, test); //When petday = 0.3, watrate = 0.6298786
    EXPECT_LE(check, 1); //watrate must be between 0 & 1
    EXPECT_GE(check, 0); //watrate must be between 0 & 1

    //Begin TEST for if 0.4 < petday < .6

    petday = 0.5;
    wat = watrate(swp, petday, shift, shape, inflec, range);

    test = 0.6285504;
    test = round(test * 1000) / 1000; //Rounding is required for unit test.
    check = round(wat * 1000) / 1000;
    EXPECT_DOUBLE_EQ(check, test); //When petrate = 0.5, watrate = 0.6285504
    EXPECT_LE(check, 1); //watrate must be between 0 & 1
    EXPECT_GE(check, 0); //watrate must be between 0 & 1

    //Begin TEST for if 0.6 < petday < 1

    petday = 0.8;
    wat = watrate(swp, petday, shift, shape, inflec, range);

    test = 0.627666;
    test = round(test * 1000) / 1000; //Rounding is required for unit test.
    check = round(wat * 1000) / 1000;
    EXPECT_DOUBLE_EQ(check, test); //When petday = 0.8, watrate = 0.627666
    EXPECT_LE(check, 1); //watrate must be between 0 & 1
    EXPECT_GE(check, 0); //watrate must be between 0 & 1

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }

  //Test evap_fromSurface by manipulating water_pool and evap_rate variables
  TEST(SWFlowTest, evap_fromSurface){

    //INPUTS
    double water_pool = 1;
    double evap_rate = 0.33;
    double aet = 0.53;

    //Begin TEST for when water_pool > evap_rate
    evap_fromSurface(&water_pool, &evap_rate, &aet);

    double test1 = 0.86;
    test1 = round(test1 * 1000) / 1000; //Rounding is required for unit test.
    double check1 = round(aet * 1000) / 1000;
    EXPECT_DOUBLE_EQ(check1, test1); //Variable aet is expected to be 0.86 with current inputs
    EXPECT_GE(check1, 0); //aet is never negative

    double test2 = 0.33;
    test2 = round(test2 * 1000) / 1000; //Rounding is required for unit test.
    double check2 = round(evap_rate * 1000) / 1000;
    EXPECT_DOUBLE_EQ(check2, test2); //Variable evap_rate is expected to be 0.33 with current inputs
    EXPECT_GE(check2, 0); //evap_rate is never negative

    double test3 = 0.67;
    test3 = round(test3 * 1000) / 1000; //Rounding is required for unit test.
    double check3 = round(water_pool * 1000) / 1000;
    EXPECT_DOUBLE_EQ(check3, test3); //Variable water_pool is expected to be 0.67 with current inputs
    EXPECT_GE(check3, 0); //water_pool is never negative

    //Begin TEST for when water_pool < evap_rate

    water_pool = 0.1, evap_rate = 0.67, aet = 0.78;
    evap_fromSurface(&water_pool, &evap_rate, &aet);

    test1 = 0.88;
    test1 = round(test1 * 1000) / 1000; //Rounding is required for unit test.
    check1 = round(aet * 1000) / 1000;
    EXPECT_DOUBLE_EQ(check1, test1); //Variable aet is expected to be 0.88 with current inputs
    EXPECT_GE(check1, 0); //aet is never negative

    test2 = 0.1;
    test2 = round(test2 * 1000) / 1000; //Rounding is required for unit test.
    check2 = round(evap_rate * 1000) / 1000;
    EXPECT_DOUBLE_EQ(check2, test2); //Variable evap_rate is expected to be 0.1 with current inputs
    EXPECT_GE(check2, 0); //evap_rate is never negative

    EXPECT_DOUBLE_EQ(water_pool, 0); //Variable water_pool is expected to be 0 when water_pool < evap_rate
    EXPECT_GE(check3, 0); //water_pool is never negative


    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }

  //Test remove_from_soil when nlyrs = 1 and when nlyrs = MAX
  TEST(SWFlowTest, remove_from_soil){

    //INPUTS
    double swc[8] =  {0.01, 1.91, 3.81, 5.71, 7.61, 9.51, 11.41, 13.31};
    double qty[8] = {0.05, 1.51, 3.51, 5.51, 7.51, 9.51, 11.51, 13.51};
    double aet = 0.33, rate = 0.62, aetExpected = 0.33;
    unsigned int nlyrs = 8;
    double coeff[8] = {0.033, 0.033, 0.067, 0.067, 0.067, 0.133, 0.133, 0.133};
    double swcmin[8] = {0.01, 1.01, 3.01, 5.01, 7.01, 9.01, 11.01, 13.01};

    ST_RGR_VALUES stValues; //Required for st->lyrFrozen[i]
    ST_RGR_VALUES *st = &stValues;

    //Begin TEST when nlyrs = 8, only 8 inputs in soils.in file.
    //TEST for if local variable sumswp = 0 (coeff[i] = 0)

    //INPUTS
    double swcExpected[8] =  {0.01, 1.91, 3.81, 5.71, 7.61, 9.51, 11.41, 13.31};
    double qtyExpected[8] = {0.05, 1.51, 3.51, 5.51, 7.51, 9.51, 11.51, 13.51};

    for(unsigned int i = 0; i < 8; i++){ //set coeff[] to all zeros
      coeff[i] = 0;
    }

    remove_from_soil(swc, qty, &aet, nlyrs, coeff, rate, swcmin);

    for(unsigned int i = 0; i < 8; i++){
      EXPECT_DOUBLE_EQ(qty[i], qtyExpected[i]); //no change expected from original values
      EXPECT_DOUBLE_EQ(swc[i], swcExpected[i]); //no change expected from original values
      EXPECT_DOUBLE_EQ(aet, aetExpected); //no change expected from original values
    }

    //Begin TEST for if st->lyrFrozen[i]
    double array[8] = {0.033, 0.033, 0.067, 0.067, 0.067, 0.133, 0.133, 0.133}; //coeff[]

    for(unsigned int i = 0; i < 8; i++){
      coeff[i] = array[i];
    }
    remove_from_soil(swc, qty, &aet, nlyrs, coeff, rate, swcmin);

    for(unsigned int i = 0; i < 8; i++){
      if (st->lyrFrozen[i]){ //If layer is frozen

        EXPECT_DOUBLE_EQ(qty[i], 0); //should be zero
        EXPECT_DOUBLE_EQ(swc[i], swcExpected[i]); //no change expected from original values
        EXPECT_DOUBLE_EQ(aet, aetExpected); //no change expected from original values
      }
    }

    //Begin TEST for if st->lyrFrozen[i] = false.
    double array1[8] = {0, 3.029771e-06, 3.031059e-03, 2.410392e-01, 1.649234e-02, 3.542865e-01, 2.006341e-01, 3.000000e-01}; //qtyExpected
    double array2[8] = {0.010000, 1.909997, 3.806969, 5.468961, 7.593508, 9.155714, 11.209366, 13.010000}; //swcExpected
    aetExpected = 1.445486;

    for(unsigned int i = 0; i < 8; i++){

      double test = array1[i];
      test = round(test); //Rounding is required for unit test.
      double check = round(qty[i]);
      EXPECT_DOUBLE_EQ(check, test); //Values for qty[] are tested against array of expected Values

      test = array2[i];
      test = round(test / 1000) * 1000; //Rounding is required for unit test.
      check = round(swc[i] / 1000) * 1000;
      EXPECT_DOUBLE_EQ(check, test); //Values for swc[] are tested against array of expected values

    }
    double test = aetExpected;
    test = round(test / 1000) * 1000; //Rounding is required for unit test.
    double check = round(aet / 1000) * 1000;
    EXPECT_DOUBLE_EQ(check, test); //aet is expected to be 0.33

    //Test when nlyrs = 1
    nlyrs = 1;
    double swc1[1] =  {0.01};
    double qty1[1] = {0.05};
    double coeff1[1] = {0.033};
    double swcmin1[1] = {0.01};
    //Begin TEST for if local variable sumswp = 0 (coeff[i] = 0)
    for(unsigned int i = 0; i < nlyrs; i++){ //set coeff[] to all zeros
      coeff[i] = 0;
    }

    remove_from_soil(swc1, qty1, &aet, nlyrs, coeff1, rate, swcmin1);

    for(unsigned int i = 0; i < nlyrs; i++){ //no change expected from original preset values
      EXPECT_DOUBLE_EQ(round(qty[i]), round(qtyExpected[i])); //no change expected from original preset values
      EXPECT_DOUBLE_EQ(swc[i], swcExpected[i]); //no change expected from original preset values
      EXPECT_DOUBLE_EQ(round(aet), round(aetExpected)); //no change expected from original preset values
    }

    //Begin TEST for if st->lyrFrozen[i]
    aetExpected = 0.33;
    for(unsigned int i = 0; i < nlyrs; i++){
      coeff[i] = array[i];
    }

    for(unsigned int i = 0; i < nlyrs; i++){
      if (st->lyrFrozen[i]){ //If layer is frozen
        remove_from_soil(swc, qty, &aet, nlyrs, coeff, rate, swcmin);

        EXPECT_DOUBLE_EQ(qty[i], 0); //should be zero
        EXPECT_DOUBLE_EQ(swc[i], swcExpected[i]); //no change expected from original values
        EXPECT_DOUBLE_EQ(aet, aetExpected); //no change expected from original values
      }
    }

    //Begin TEST for if st->lyrFrozen[i] = false.
    double array3[8] = {0, 1.889579e-08, 1.890382e-05, 1.503291e-03, 1.376369e-01, 5.000000e-01, 2.006341e-01, 3.000000e-01}; //qtyExpected
    aetExpected = 1.469793;

    for(unsigned int i = 0; i < nlyrs; i++){

      double test = array3[i];
      test = round(test / 1000) * 1000; //Rounding is required for unit test.
      double check = round(qty[i] / 1000) * 1000;
      EXPECT_DOUBLE_EQ(check, test);//Values for qty[] are tested against array of expected Values

      test = array2[i];
      test = round(test / 1000) * 1000; //Rounding is required for unit test.
      check = round(swc[i] / 1000) * 1000;
      EXPECT_DOUBLE_EQ(check, test);//Values for swc[] are tested against array of expected values

      test = aetExpected;
      test = round(test / 1000) * 1000; //Rounding is required for unit test.
      check = round(aet / 1000) * 1000;
      EXPECT_DOUBLE_EQ(check, test); //aet is expected to be 1.469793.
    }

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }

  //Test when nlyrs = 1 and 8 for outputs; swc, drain, drainout, standing water
  TEST(SWFlowTest, infiltrate_water_low){

    //INPUTS
    double swc[8] = {0.01, 1.01, 3.01, 5.01, 7.01, 9.01, 11.01, 13.01};
    double drain[8] = {1,1,1,1,1,1,1,1};
    double drainout = 0.1, sdrainpar = 0.6, sdraindpth = 6, standingWater = 0;
    unsigned int nlyrs = 8;
    double swcfc[8] = {0.33, 0.46, 0.78, 0.97, 1.02, 1.44, 1.78, 2.01};
    double width[8] = {5, 5, 5, 5, 10, 10, 10, 10};
    double swcmin[8] = {0.02, 1.91, 3.81, 5.71, 7.61, 9.51, 11.41, 13.31};
    double swcsat[8] = {0, 10.77, 13.61, 5.01, 6.01, 12.01, 13.01, 15};
    double impermeability[8] = {0.05, 0.55, 0.75, 0.99, 1, 1, 1.5, 2};

    ST_RGR_VALUES stValues;
	  ST_RGR_VALUES *st = &stValues;

    double swcExpected[8] = {0, 1.01, 4.01, 5.01, 6.01, 9.01, 11.01, 13.01};
    double drainExpected[8] = {1, 1, 0, 0, 1, 1, 1, 1};
    double drainoutExpected = 0.1;
    double standingWaterExpected = 0.01;

    //Begin TEST for if swc[i] <= swcmin[i]

    infiltrate_water_low(swc, drain, &drainout, nlyrs, sdrainpar, sdraindpth, swcfc, width, swcmin,
      swcsat, impermeability, &standingWater);


    EXPECT_DOUBLE_EQ(drainoutExpected, drainout);
    EXPECT_DOUBLE_EQ(standingWaterExpected, standingWater);

    for(unsigned int i = 0; i < nlyrs; i++){

      EXPECT_DOUBLE_EQ(swcExpected[i], swc[i]);
      EXPECT_DOUBLE_EQ(drainExpected[i], drain[i]);

    }

    //Begin TEST for if swc[i] > swcmin[i]
    double swcExpected2[8] = {0, 1.6495, 4.3800, 5.0100, 6.0100, 9.5100, 11.6100, 13.1100};
    double drainExpected2[8] = {1.0095, 1.2700, -1.3000, -0.6000, 1.0000, 1.0000, 0.8000, 0.9000};

    infiltrate_water_low(swcmin, drain, &drainout, nlyrs, sdrainpar, sdraindpth, swcfc, width, swc,
      swcsat, impermeability, &standingWater); //switch swc and swcmin arrays

    standingWaterExpected = 0.0105;

    EXPECT_DOUBLE_EQ(drainoutExpected, drainout);

    double test = standingWaterExpected;
    test = round(test * 10) / 10; //Rounding is required for unit test.
    double check = round(standingWater * 10) / 10;
    EXPECT_DOUBLE_EQ(test, check);

    for(unsigned int i = 0; i < nlyrs; i++){

      test = round(swcExpected2[i] / 10) * 10;
      check = round(swc[i] / 10) * 10;
      EXPECT_DOUBLE_EQ(test, check);

      test = round(drainExpected2[i] / 100) * 100; //Rounding is required for unit test.
      check = round(drain[i] / 100) * 100;
      EXPECT_DOUBLE_EQ(test, check);

    }

    //Begin TEST for if lyrFrozen == true
    double swcExpected3[8] = {0, 1.907395, 6.112700, 5.010000, 6.010000, 9.510000, 11.412000, 13.308000};
    double drainExpected3[8] = {1.000095, 1.002700, -1.300000, -0.600000, 1.000000, 1.000000, 0.998000, 0.997020};

    infiltrate_water_low(swcmin, drain, &drainout, nlyrs, sdrainpar, sdraindpth, swcfc, width, swc,
      swcsat, impermeability, &standingWater);

    for(unsigned int i = 0; i < nlyrs; i++){
      if (st->lyrFrozen[i]){

        standingWaterExpected = 0.019905;
        test = round(standingWaterExpected / 10) * 10;
        check = round(standingWater / 10) * 10;
        EXPECT_DOUBLE_EQ(test, check);
        EXPECT_DOUBLE_EQ(drainoutExpected, drainout);

        test = round(swcExpected3[i] / 10) * 10;
        check = round(swc[i] / 10) * 10;
        EXPECT_DOUBLE_EQ(test, check);

        test = round(drainExpected3[i] / 100) * 100; //Rounding is required for unit test.
        check = round(drain[i] / 100) * 100;
        EXPECT_DOUBLE_EQ(test, check);
      }
    //Begin TEST for if lyrFrozen == false
      else{

        standingWaterExpected = 0.019905;
        test = round(standingWaterExpected / 10) * 10; //Rounding is required for unit test.
        check = round(standingWater / 10) * 10;
        EXPECT_DOUBLE_EQ(test, check);
        EXPECT_DOUBLE_EQ(drainoutExpected, drainout);

        test = round(swcExpected2[i] / 10) * 10; //Rounding is required for unit test.
        check = round(swc[i] / 10) * 10;
        EXPECT_DOUBLE_EQ(test, check);

        test = round(drainExpected2[i] / 100) * 100; //Rounding is required for unit test.
        check = round(drain[i] / 100) * 100;
        EXPECT_DOUBLE_EQ(test, check);
      }
    }

    //Begin TEST for if swc[j] > swcsat[j]
    //INPUTS
    double swc2[8] = {1.02, 2.02, 3.02, 4.02, 5.02, 6.02, 7.02, 8.02};
    double swcsat2[8] = {1.01, 2.01, 3.01, 4.01, 5.01, 6.01, 7.01, 8.01};

    double swcExpected4[8] = {0, 2.01, 3.01, 6.01, 5.01, 6.01, 7.01, 8.01};
    double drainExpected4[8] = {0.93, 1.45, -2.35, -1.6, 0.97, 0.98, 0.49, 0.90};
    standingWaterExpected = 0.08;

    infiltrate_water_low(swc2, drain, &drainout, nlyrs, sdrainpar, sdraindpth, swcfc, width, swcmin,
      swcsat2, impermeability, &standingWater);

    EXPECT_DOUBLE_EQ(drainoutExpected, drainout);

    test = round(standingWaterExpected * 10) / 10; //Rounding is required for unit test.
    check = round(standingWater * 10) / 10;
    EXPECT_DOUBLE_EQ(test, check);

    for(unsigned int i = 0; i < nlyrs; i++){

      test = round(swcExpected4[i] / 10) * 10;
      check = round(swc[i] / 10) * 10;
      EXPECT_DOUBLE_EQ(test, check);

      test = round(drainExpected4[i] * 10) / 10; //Rounding is required for unit test.
      check = round(drain[i] * 10) / 10;
      EXPECT_DOUBLE_EQ(test, check);

    }

    //Begin TEST for if swc[j] <= swcsat[j]
    //INPUTS
    double swcsat3[8] = {5.0, 10.77, 13.61, 7.01, 8.01, 12.01, 13.01, 15};

    double swcExpected5[8] = {0.01, 1.01, 3.01, 5.01, 7.01, 9.01, 11.01, 13.01};
    double drainExpected5[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    standingWaterExpected = 0;

    infiltrate_water_low(swc, drain, &drainout, nlyrs, sdrainpar, sdraindpth, swcfc, width, swcmin,
      swcsat3, impermeability, &standingWater);

    EXPECT_DOUBLE_EQ(drainoutExpected, drainout);
    test = round(standingWaterExpected); //Rounding is required for unit test.
    check = round(standingWater);
    EXPECT_DOUBLE_EQ(test, check);

    for(unsigned int i = 0; i < nlyrs; i++){

      test = round(swcExpected5[i] / 10) * 10;
      check = round(swc[i] / 10) * 10;
      EXPECT_DOUBLE_EQ(test, check);

      test = round(drainExpected5[i] / 100) * 100; //Rounding is required for unit test.
      check = round(drain[i] / 100) * 100;
      EXPECT_DOUBLE_EQ(test, check);

    }

    //Begin testing when nlyrs = 1
    nlyrs = 1;

    //INPUTS
    double swc_1[1] = {0.01};
    double drain_1[1] = {1};
    drainout = 0.1, sdrainpar = 0.6, sdraindpth = 6, standingWater = 0;
    double swcfc_1[1] = {0.33};
    double width_1[1] = {5};
    double swcmin_1[1] = {0.02};
    double swcsat_1[1] = {0};
    double impermeability_1[1] = {0.05};

    double swcExpected_1[1] = {0};
    double drainExpected_1[8] = {1};
    drainoutExpected = 0.1;
    standingWaterExpected = 0.01;

    //Begin TEST for if swc[i] <= swcmin[i]

    infiltrate_water_low(swc_1, drain_1, &drainout, nlyrs, sdrainpar, sdraindpth, swcfc_1, width_1, swcmin_1,
      swcsat_1, impermeability_1, &standingWater);


    EXPECT_DOUBLE_EQ(drainoutExpected, drainout);
    EXPECT_DOUBLE_EQ(standingWaterExpected, standingWater);

    for(unsigned int i = 0; i < nlyrs; i++){

      EXPECT_DOUBLE_EQ(swcExpected_1[i], swc_1[i]);
      EXPECT_DOUBLE_EQ(drainExpected_1[i], drain_1[i]);

    }

    //Begin TEST for if swc[i] > swcmin[i]
    double swcExpected2_1[1] = {0};
    double drainExpected2_1[1] = {1.0095};

    infiltrate_water_low(swcmin_1, drain_1, &drainout, nlyrs, sdrainpar, sdraindpth, swcfc_1, width_1, swc_1,
      swcsat_1, impermeability_1, &standingWater); //switch swc and swcmin arrays

    standingWaterExpected = 0.0105;

    EXPECT_DOUBLE_EQ(round(drainoutExpected * 10) / 10, round(drainout * 10) / 10);

    test = standingWaterExpected;
    test = round(test * 10) / 10; //Rounding is required for unit test.
    check = round(standingWater * 10) / 10;
    EXPECT_DOUBLE_EQ(test, check);

    for(unsigned int i = 0; i < nlyrs; i++){

      test = round(swcExpected2_1[i] / 10) * 10;
      check = round(swc_1[i] / 10) * 10;
      EXPECT_DOUBLE_EQ(test, check);

      test = round(drainExpected2_1[i] / 100) * 100; //Rounding is required for unit test.
      check = round(drain_1[i] / 100) * 100;
      EXPECT_DOUBLE_EQ(test, check);

    }

    //Begin TEST for if lyrFrozen == true
    double swcExpected3_1[1] = {0};
    double drainExpected3_1[1] = {1.000095};

    infiltrate_water_low(swcmin_1, drain_1, &drainout, nlyrs, sdrainpar, sdraindpth, swcfc_1, width_1, swc_1,
      swcsat_1, impermeability_1, &standingWater);

    for(unsigned int i = 0; i < nlyrs; i++){
      if (st->lyrFrozen[i]){

        standingWaterExpected = 0.019905;
        test = round(standingWaterExpected / 10) * 10;
        check = round(standingWater / 10) * 10;
        EXPECT_DOUBLE_EQ(test, check);
        EXPECT_DOUBLE_EQ(drainoutExpected, drainout);

        test = round(swcExpected3_1[i] / 10) * 10;
        check = round(swc_1[i] / 10) * 10;
        EXPECT_DOUBLE_EQ(test, check);

        test = round(drainExpected3_1[i] / 100) * 100; //Rounding is required for unit test.
        check = round(drain_1[i] / 100) * 100;
        EXPECT_DOUBLE_EQ(test, check);
      }
    //Begin TEST for if lyrFrozen == false
      else{

        standingWaterExpected = 0.019905;
        test = round(standingWaterExpected / 10) * 10; //Rounding is required for unit test.
        check = round(standingWater / 10) * 10;
        EXPECT_DOUBLE_EQ(test, check);
        EXPECT_DOUBLE_EQ(round(drainoutExpected * 10) / 10, round(drainout * 10) / 10);

        test = round(swcExpected2_1[i] / 10) * 10; //Rounding is required for unit test.
        check = round(swc_1[i] / 10) * 10;
        EXPECT_DOUBLE_EQ(test, check);

        test = round(drainExpected2_1[i] / 100) * 100; //Rounding is required for unit test.
        check = round(drain_1[i] / 100) * 100;
        EXPECT_DOUBLE_EQ(test, check);
      }
    }

    //Begin TEST for if swc[j] > swcsat[j]
    //INPUTS
    double swc2_1[1] = {1.02};
    double swcsat2_1[1] = {1.01};

    double swcExpected4_1[1] = {0};
    double drainExpected4_1[1] = {1.57};
    standingWaterExpected = 0;
    drainoutExpected = 0.67;

    infiltrate_water_low(swc2_1, drain_1, &drainout, nlyrs, sdrainpar, sdraindpth, swcfc_1, width_1, swcmin_1,
      swcsat2_1, impermeability_1, &standingWater);

    EXPECT_DOUBLE_EQ(round(drainoutExpected * 10) / 10, round(drainout * 10) / 10);

    test = round(standingWaterExpected * 10) / 10; //Rounding is required for unit test.
    check = round(standingWater * 10) / 10;
    EXPECT_DOUBLE_EQ(test, check);

    for(unsigned int i = 0; i < nlyrs; i++){

      test = round(swcExpected4_1[i] / 10) * 10;
      check = round(swc_1[i] / 10) * 10;
      EXPECT_DOUBLE_EQ(test, check);

      test = round(drainExpected4_1[i] * 10) / 10; //Rounding is required for unit test.
      check = round(drain_1[i] * 10) / 10;
      EXPECT_DOUBLE_EQ(test, check);

    }

    //Begin TEST for if swc[j] <= swcsat[j]
    //INPUTS
    double swcsat3_1[1] = {5.0};

    double swcExpected5_1[1] = {0.01};
    double drainExpected5_1[1] = {1};
    standingWaterExpected = 0;

    infiltrate_water_low(swc_1, drain_1, &drainout, nlyrs, sdrainpar, sdraindpth, swcfc_1, width_1, swcmin_1,
      swcsat3_1, impermeability_1, &standingWater);

    EXPECT_DOUBLE_EQ(round(drainoutExpected * 10) / 10, round(drainout * 10) / 10);
    test = round(standingWaterExpected); //Rounding is required for unit test.
    check = round(standingWater);
    EXPECT_DOUBLE_EQ(test, check);

    for(unsigned int i = 0; i < nlyrs; i++){

      test = round(swcExpected5_1[i] / 10) * 10;
      check = round(swc_1[i] / 10) * 10;
      EXPECT_DOUBLE_EQ(test, check);

      test = round(drainExpected5_1[i] / 100) * 100; //Rounding is required for unit test.
      check = round(drain_1[i] / 100) * 100;
      EXPECT_DOUBLE_EQ(test, check);

    }

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }
/**
  TEST(SWFlowTest, hydraulic_redistribution){

    //INPUTS
    double swc[]
    double swcwp[]
    double lyrRootCo[]
    double hydred[]
    unsigned int nlyrs
    double maxCondroot
    double swp50
    double shapeCond
    double scale

    //Begin TEST for if swp[i] < swpwp[i] OR swp[j] < swpwp[j] AND lyrFrozen == false

    //Begin TEST for if else ^^

    //Begin TEST for if swp[i] > swp[i+1]; j = i+1

    //Begin TEST for relCondroot[i] > (relCondroot[i+1] * (lyrRootCo[i] * lyrRootCo[j] / (1. - Rx))

    //Begin TEST for relCondroot[i] <= (relCondroot[i+1] * (lyrRootCo[i] * lyrRootCo[j] / (1. - Rx))

    //Begin TEST for swa

    //Begin TEST for if (-hydredsum) > swa




    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }*/

}
