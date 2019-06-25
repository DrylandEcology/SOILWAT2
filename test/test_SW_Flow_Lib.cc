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
#include "../pcg/pcg_basic.h"

#include "../SW_Flow_lib.h"

#include "sw_testhelpers.h"

extern SW_SITE SW_Site;
extern SW_MODEL SW_Model;
extern SW_VEGPROD SW_VegProd;
extern ST_RGR_VALUES stValues;
//extern SW_SOILWAT_OUTPUTS SW_Soilwat_outputs;

pcg32_random_t flow_rng;
SW_VEGPROD *v = &SW_VegProd;
SW_SITE *s = &SW_Site;
ST_RGR_VALUES *st = &stValues;
//SW_SOILWAT_OUTPUTS *swo = NULL;

int k;

namespace
{

  // Test the veg interception function 'veg_intercepted_water'
  TEST(SWFlowTest, VegInterceptedWater)
  {

    ForEachVegType(k)
    {
      // declare inputs
      double bLAI, ppt, pptleft, wintveg, store;
      double scale = 1.0, m = 1.0;

      // Test expectation when there is no leaf-area
      bLAI = 0.0, ppt = 5.0, pptleft = ppt, store = 0.0;

      veg_intercepted_water(&pptleft, &wintveg, &store,
        m, v->veg[k].veg_kSmax, bLAI, scale);

      EXPECT_EQ(0, wintveg); // When there is no veg, interception should be 0
      EXPECT_EQ(0, store); // When there is no veg, stored interception should be 0
      EXPECT_EQ(pptleft, ppt); /* When there is no interception, ppt before interception
        should equal ppt left after interception */

      // Test expectations when there is no rain, but there is leaf-area
      bLAI = 1.5, ppt = 0.0, pptleft = ppt, store = 0.0;

      veg_intercepted_water(&pptleft, &wintveg, &store,
        m, v->veg[k].veg_kSmax, bLAI, scale);

      EXPECT_EQ(0, wintveg);  // When there is no ppt, interception should be 0
      EXPECT_EQ(0, store);  // When there is no ppt, stored interception should be 0
      EXPECT_EQ(pptleft, ppt); /* When there is no interception, ppt before interception
        should equal ppt left after interception */

      // Test expectations when there is both veg cover and precipitation
      bLAI = 1.5, ppt = 5.0, pptleft = ppt, store = 0.0;

      veg_intercepted_water(&pptleft, &wintveg, &store,
        m, v->veg[k].veg_kSmax, bLAI, scale);

      EXPECT_GT(wintveg, 0); // interception by veg should be greater than 0
      EXPECT_LE(wintveg, ppt); // interception by veg should be less than or equal to ppt
      EXPECT_GT(store, 0); // stored interception by veg should be greater than 0
      EXPECT_GE(pptleft, 0); // The pptleft (for soil) should be greater than or equal to 0

      // Reset to previous global state
      Reset_SOILWAT2_after_UnitTest();
    }
  }

  // Test the litter interception function 'litter_intercepted_water'
  TEST(SWFlowTest, LitterInterceptedWater)
  {

    ForEachVegType(k)
    {
      // declare inputs
      double blitter, ppt, pptleft, wintlit, store;
      double scale = 1.0, m = 1.0;

      // Test expectation when there is no litter
      blitter = 0.0, ppt = 5.0, pptleft = ppt, wintlit = 0.0, store = 0.0;

      litter_intercepted_water(&pptleft, &wintlit, &store,
        m, v->veg[k].lit_kSmax, blitter, scale);

      EXPECT_EQ(0, wintlit); // When litter is 0, interception should be 0
      EXPECT_EQ(0, store); // When litter is 0, stored interception should be 0
      EXPECT_EQ(pptleft, ppt); /* When litter is 0, ppt before interception
        should equal ppt left after interception */

      // Test expectations when pptleft is 0
      pptleft = 0.0, scale = 0.5, blitter = 5.0;


      // Test expectations when there is no throughfall
      blitter = 200.0, ppt = 0.0, pptleft = ppt, wintlit = 0.0, store = 0.0;

      litter_intercepted_water(&pptleft, &wintlit, &store,
        m, v->veg[k].lit_kSmax, blitter, scale);

      EXPECT_EQ(0, pptleft); // When there is no ppt, pptleft should be 0
      EXPECT_EQ(0, wintlit); // When there is no ppt, interception should be 0
      EXPECT_EQ(0, store); // When there is no ppt, stored interception should be 0

      //litter_intercepted_water(&pptleft, &wintlit, blitter, scale, a, b, c, d);

      // Test expectations when pptleft, scale, and blitter are greater than 0
      blitter = 200.0, ppt = 5.0, pptleft = ppt, wintlit = 0.0, store = 0.0;

      litter_intercepted_water(&pptleft, &wintlit, &store,
        m, v->veg[k].lit_kSmax, blitter, scale);

      EXPECT_GT(wintlit, 0); // interception by litter should be greater than 0
      EXPECT_LE(wintlit, pptleft); // interception by lit should be less than or equal to ppt
      EXPECT_GT(store, 0); // stored interception by litter should be greater than 0
      EXPECT_GE(pptleft, 0); // The pptleft (for soil) should be greater than or equal to 0

      // Reset to previous global state
      Reset_SOILWAT2_after_UnitTest();
    }
  }

  // Test infiltration under high water function, 'infiltrate_water_high'
  TEST(SWFlowTest, infiltrate_water_high)
  {

    // declare inputs
    double pptleft = 5.0, standingWater, drainout;

    // ***** Tests when nlyrs = 1 ***** //
    ///  provide inputs
    int nlyrs = 1;
    double swc[1] = {0.8}, swcfc[1] = {1.1}, swcsat[1] = {1.6},
      impermeability[1] = {0.}, drain[1] = {0.};

    infiltrate_water_high(swc, drain, &drainout, pptleft, nlyrs, swcfc, swcsat,
      impermeability, &standingWater);

    EXPECT_GE(drain[0], 0); // drainage should be greater than or equal to 0 when soil layers are 1 and ppt > 1
    EXPECT_LE(swc[0], swcsat[0]); // swc should be less than or equal to swcsat
    EXPECT_DOUBLE_EQ(drainout, drain[0]); // drainout and drain should be equal when we have one layer

    /* Test when pptleft and standingWater are 0 (No drainage) */
    pptleft = 0.0, standingWater = 0.0, drain[0] = 0., swc[0] = 0.8,
    swcfc[0] = 1.1, swcsat[0] = 1.6;

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
    unsigned int i;
    nlyrs = MAX_LAYERS, pptleft = 5.0;
    double *swc2 = new double[nlyrs];
    double *swcfc2 = new double[nlyrs];
    double *swcsat2 = new double[nlyrs];
    double *impermeability2 = new double[nlyrs];
    double *drain2 = new double[nlyrs];

    pcg32_random_t infiltrate_rng;
    RandSeed(0,&infiltrate_rng);

    for (i = 0; i < MAX_LAYERS; i++)
    {
      swc2[i] = RandNorm(1.,0.5,&infiltrate_rng);
      swcfc2[i] = RandNorm(1,.5,&infiltrate_rng);
      swcsat2[i] = swcfc2[i] + .1; // swcsat will always be greater than swcfc in each layer
      impermeability2[i] = 0.0;
    }

    infiltrate_water_high(swc2, drain2, &drainout, pptleft, nlyrs, swcfc2,
      swcsat2, impermeability2, &standingWater);

    EXPECT_EQ(drainout, drain2[MAX_LAYERS - 1]); /* drainout and drain should be
      equal in the last layer */

    for (i = 0; i < MAX_LAYERS; i++)
    {
      EXPECT_LE(swc2[i], swcsat2[i]); // swc should be less than or equal to swcsat
      EXPECT_GE(drain2[i], 0.); /*  drainage should be greater than or
      equal to 0 or a very small value like 0 */
    }

    /* Test when pptleft and standingWater are 0 (No drainage); swc < swcfc3  < swcsat */
    pptleft = 0.0, standingWater = 0.0;
    double *swc3 = new double[nlyrs];
    double *swcfc3 = new double[nlyrs];
    double *swcsat3 = new double[nlyrs];
    double *drain3 = new double[nlyrs];

    for (i = 0; i < MAX_LAYERS; i++)
    {
      swc3[i] = RandNorm(1.,0.5,&infiltrate_rng);
      swcfc3[i] = swc3[i] + .2;
      swcsat3[i] = swcfc3[i] + .5;
      drain3[i] = 0.;// swcsat will always be greater than swcfc in each layer
    }

    infiltrate_water_high(swc3, drain3, &drainout, pptleft, nlyrs, swcfc3,
      swcsat3, impermeability2, &standingWater);

    for (i = 0; i < MAX_LAYERS; i++)
    {
      EXPECT_DOUBLE_EQ(0, drain3[i]); // drainage should be 0
    }

    /* Test when impermeability is greater than 0 and large precipitation */
    double *impermeability4 = new double[nlyrs];
    double *drain4 = new double[nlyrs];
    double *swc4 = new double[nlyrs];
    double *swcfc4 = new double[nlyrs];
    double *swcsat4 = new double[nlyrs];
    pptleft = 20.0;

    for (i = 0; i < MAX_LAYERS; i++)
    {
      swc4[i] = RandNorm(1.,0.5,&infiltrate_rng);
      swcfc4[i] = swc4[i] + .2;
      swcsat4[i] = swcfc4[i] + .3; // swcsat will always be greater than swcfc in each layer
      impermeability4[i] = 1.0;
      drain4[i] = 0.0;
    }

    swc4[0] = 0.8; // Need to hard code this value because swc4 is altered by function

    infiltrate_water_high(swc4, drain4, &drainout, pptleft, nlyrs, swcfc4,
      swcsat4, impermeability4, &standingWater);

    EXPECT_DOUBLE_EQ(standingWater, (pptleft + 0.8) - swcsat4[0]); /* When impermeability is 1,
      standingWater should be equivalent to  pptLeft + swc[0] - swcsat[0]) */

    for (i = 0; i < MAX_LAYERS; i++)
    {
      EXPECT_DOUBLE_EQ(0, drain4[i]); //When impermeability is 1, drainage should be 0
    }

    /* Test "push", when swcsat > swc */
    double *impermeability5 = new double[nlyrs];
    double *drain5 = new double[nlyrs];
    double *swc5 = new double[nlyrs];
    double *swcfc5 = new double[nlyrs];
    double *swcsat5 = new double[nlyrs];
    pptleft = 5.0;

    for (i = 0; i < MAX_LAYERS; i++)
    {
      swc5[i] = RandNorm(1.2,0.5,&infiltrate_rng);
      swcfc5[i] = swc5[i] - .4; // set up conditions for excess SWC
      swcsat5[i] = swcfc5[i] + .1; // swcsat will always be greater than swcfc in each layer
      impermeability5[i] = 1.0;
      drain5[i] = 0.0;
    }

    infiltrate_water_high(swc5, drain5, &drainout, pptleft, nlyrs, swcfc5,
      swcsat5, impermeability5, &standingWater);

    for (i = 0; i < MAX_LAYERS; i++)
    {
      EXPECT_NEAR(swc5[i], swcsat5[i], tol6); // test that swc is now equal to or below swcsat in all layers but the top
    }

    EXPECT_GT(standingWater, 0); // standingWater should be above 0


    // deallocate pointers
    double *array_list[] = { impermeability2, drain2, swc2, swcfc2, swcsat2,
                           drain3, swc3, swcfc3, swcsat3,
                           impermeability4, drain4, swc4, swcfc4, swcsat4,
                           impermeability5, drain5, swc5, swcfc5, swcsat5 };
    for (i = 0; i < length(array_list); i++)
    {
      delete[] array_list[i];
    }

    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }


  //Test svapor function by manipulating variable temp.
  TEST(SWFlowTest, svapor)
  {
    //Declare INPUTS
    double temp[] = {30,35,40,45,50,55,60,65,70,75,20,-35,-12.667,-1,0}; // These are test temperatures, in degrees Celcius.
    double expOut[] = {32.171, 43.007, 56.963, 74.783, 97.353, 125.721, 161.113,
                       204.958, 258.912, 324.881, 17.475, 0.243, 1.716, 4.191,
                       4.509}; // These are the expected outputs for the svapor function.

    //Declare OUTPUTS
    double vapor;

    //Begin TEST
    for (int i = 0; i <15; i++) {
      vapor = svapor(temp[i]);

      EXPECT_NEAR(expOut[i], vapor, tol3); // Testing input array temp[], expected output is expOut[].
    }

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }

  //Test petfunc by manipulating each input individually.
  TEST(SWFlowTest, petfunc)
  {
    //Begin TEST for avgtemp input variable
    //Declare INPUTS
    unsigned int doy = 2; //For the second day in the month of January
    double rlat = 0.681, elev = 1000, slope = 0, aspect = -1, reflec = 0.15,
      humid = 61,windsp = 1.3, cloudcov = 71, transcoeff = 1;
    double temp, check;
    double avgtemps[] = {30,35,40,45,50,55,60,65,70,75,20,-35,-12.667,-1,0}; // These are test temperatures, in degrees Celcius.

    //Declare INPUTS for expected returns
    double expReturnTemp[] = {0.201, 0.245, 0.299, 0.364, 0.443, 0.539, 0.653,
                              0.788, 0.948, 1.137, 0.136, 0.01, 0.032, 0.057,
                              0.060}; // These are the expected outcomes for the variable arads.

    for (int i = 0; i <15; i++)
    {
      temp = avgtemps[i]; //Uses array of temperatures for testing for input into temp variable.
      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid,
                      windsp, cloudcov, transcoeff);

      EXPECT_NEAR(check, expReturnTemp[i], tol3); //Tests the return of the petfunc against the expected return for the petfunc.
    }

    //Begin TEST for rlat input variable.  Inputs outside the range of [-1.169,1.169] produce the same output, 0.042.  Rlat values outside this range represent values near the poles.
    //INPUTS
    temp = 25, cloudcov = 71, humid = 61, windsp = 1.3;
    double rlats[] = {-1.570796, -0.7853982, 0, 0.7853982, 1.570796}; //Initialize array of potential latitudes, in radians, for the site.

   //Declare INPUTS for expected returns
    double expReturnLats[] = {0.042, 0.420, 0.346, 0.134, 0.042}; //These are the expected returns for petfunc while manipulating the rlats input variable.

    for (int i = 0; i < 5; i++)
    {
      rlat = rlats[i]; //Uses array of latitudes initialized above.
      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid,
                      windsp, cloudcov, transcoeff);

      EXPECT_NEAR(check, expReturnLats[i], tol3); //Tests the return of the petfunc against the expected return for the petfunc.
    }
    //Begin TEST for elev input variable, testing from -413 meters (Death Valley) to 8727 meters (~Everest).
    //INPUTS
    rlat = 0.681;
    double elevT[] = {-413,0,1000,4418,8727};

    //Declare INPUTS for expected returns
    double expReturnElev[] = {0.181,0.176,0.165,0.130,0.096};

    for (int i = 0; i < 5; i++)
    {
      check = petfunc(doy, temp, rlat, elevT[i], slope, aspect, reflec, humid,
                      windsp, cloudcov, transcoeff);

      EXPECT_NEAR(check, expReturnElev[i], tol3); //Tests the return of the petfunc against the expected return for the petfunc.
    }
    //Begin TEST for slope input variable
    //INPUTS
    elev = 1000;
    double slopeT[] = {0,15,34,57,90};

    //Declare INPUTS for expected returns
    double expReturnSlope[] = {0.165, 0.082, 0.01, 0.01, 0.01};
      //Expected returns of 0.01 occur when the petfunc returns a negative number.

    for (int i = 0; i < 5; i++)
    {
      check = petfunc(doy, temp, rlat, elev, slopeT[i], aspect, reflec, humid,
                      windsp, cloudcov, transcoeff);

      EXPECT_NEAR(check, expReturnSlope[i], tol3); //Tests the return of the petfunc against the expected return for the petfunc.
    }

    //Begin TEST for aspect input variable
    //INPUTS
    slope = 5;
    double aspectT[] = {0, 46, 112, 253, 358};

    //Declare INPUTS for expected returns
    double expReturnAspect[] = {0.138, 0.146, 0.175, 0.172, 0.138};

    for (int i = 0; i < 5; i++) {
      check = petfunc(doy, temp, rlat, elev, slope, aspectT[i], reflec, humid,
                      windsp, cloudcov, transcoeff);

      EXPECT_NEAR(check, expReturnAspect[i], tol3);
    }

    //Begin TEST for reflec input variable
    //INPUTS
    aspect = -1, slope = 0;
    double reflecT[] = {0.1, 0.22, 0.46, 0.55, 0.98};

    //Declare INPUTS for expected returnsdouble expReturnSwpAvg = 0.00000148917;
    double expReturnReflec[] = {0.172, 0.155, 0.120, 0.107, 0.045};

    for (int i = 0; i < 5; i++)
    {
      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflecT[i], humid,
                      windsp, cloudcov, transcoeff);

      EXPECT_NEAR(check, expReturnReflec[i], tol3);
    }

    //Begin TEST for humidity input varibable.
    //INPUTS
    reflec = 0.15;
    double humidT[] = {2, 34, 56, 79, 89};

    //Declare INPUTS for expected returns.
    double expReturnHumid[] = {0.242, 0.218, 0.176, 0.125, 0.102};

    for (int i = 0; i < 5; i++)
    {
      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humidT[i],
                      windsp, cloudcov, transcoeff);

      EXPECT_NEAR(check, expReturnHumid[i], tol3);
    }

    //Begin TEST for windsp input variable.
    //INPUTS
    humid = 61, windsp = 0;

    //Declare INPUTS for expected returns.
    double expReturnWindsp[] = {0.112, 0.204, 0.297, 0.390, 0.483, 0.576, 0.669,
                                0.762, 0.855, 0.948, 1.041, 1.134, 1.227, 1.320,
                                1.413, 1.506, 1.599, 1.692, 1.785, 1.878, 1.971,
                                2.064, 2.157};

    for (int i = 0; i < 23; i++)
    {
      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid,
                      windsp, cloudcov, transcoeff);

      EXPECT_NEAR(check, expReturnWindsp[i], tol3);

      windsp += 2.26; //Increments windsp input variable.
    }

    //Begin TEST for cloudcov input variable.
    //INPUTS
    windsp = 1.3;
    double cloudcovT[] = {1, 12, 36, 76, 99};

    //Declare INPUTS for expected returns.
    double expReturnCloudcov[] = {0.148, 0.151, 0.157, 0.166, 0.172};

    for (int i = 0; i < 5; i++)
    {
      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid,
                      windsp, cloudcovT[i], transcoeff);

      EXPECT_NEAR(check, expReturnCloudcov[i], tol3);

      cloudcov += 4.27; //Incrments cloudcov input variable.
    }

    //Begin TEST for cloudcov input variable.
    //INPUTS
    cloudcov = 71;
    double transcoeffT[] = {0.01, 0.11, 0.53, 0.67, 0.89};

    //Declare INPUTS for expected returns.
    double expReturnTranscoeff = 0.1650042;
    //The same value is returned for every tested transcoeff input.

    for (int i = 0; i < 5; i++)
    {
      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid,
                      windsp, cloudcov, transcoeffT[i]);

      EXPECT_NEAR(check, expReturnTranscoeff, tol3);
    }

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
    }

  //Test transp_weighted_avg function.
  TEST(SWFlowTest, transp_weighted_avg)
  {
    //INPUTS
    double swp_avg = 10;
    unsigned int n_tr_rgns = 1, n_layers = 1;
    unsigned int tr_regions[1] = {1}; // 1-4
    double tr_coeff[1] = {0.0496}; //trco_grass
    double swc[1] = {12};

    //INPUTS for expected outputs
    double swp_avgExpected1 = 1.100536e-06;
    //Begin TEST when n_layers is one
    transp_weighted_avg(&swp_avg, n_tr_rgns, n_layers, tr_regions, tr_coeff,
                        swc);

    EXPECT_GE(swp_avg, 0); //Must always be non negative.
    EXPECT_NEAR(swp_avgExpected1, swp_avg, tol6); //swp_avg is expected to be 1.100536e-06

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();

    //Begin TEST when n_layers is at "max"
    //INPUTS
    swp_avg = 10, n_tr_rgns = 4, n_layers = 25;
    unsigned int i, tr_regions2[25] = {1,1,1,2,2,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,
                                       4,4,4,4,4};
    double tr_coeff2[25],swc2[25];

    //INPUTS for expected OUTPUTS
    double swp_avgExpectedM = 1.7389131503001496;

    // Setup soil layers
    create_test_soillayers(n_layers);

    ForEachSoilLayer(i)
    {
      // copy soil layer values into arrays so that they can be passed as
      // arguments to `transp_weighted_avg`
      tr_coeff2[i] = s->lyr[i]->transp_coeff[SW_SHRUB];

      // example: swc as mean of wilting point and field capacity
      swc2[i] = (s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt) / 2.;
    }


		transp_weighted_avg(&swp_avg, n_tr_rgns, n_layers, tr_regions2, tr_coeff2,
                        swc2);

		EXPECT_GE(swp_avg, 0); //Must always be non negative.


		EXPECT_NEAR(swp_avgExpectedM, swp_avg, tol6);

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }


  //Test EsT_partitioning by manipulating fbse and fbst variables.
  TEST(SWFlowTest, EsT_partitioning)
  {
    //INPUTS
    double fbse = 0, fbst = 0, blivelai = 0.002, lai_param = 2;

    //TEST when fbse > bsemax
    double fbseExpected = 0.995, fbstExpected = 0.005;
    EsT_partitioning(&fbse, &fbst, blivelai, lai_param);

    EXPECT_NEAR(fbse, fbseExpected, tol6); //fbse is expected to be 0.995
    EXPECT_NEAR(fbst, fbstExpected, tol6); //fbst = 1 - fbse; fbse = bsemax
    EXPECT_GE(fbse, 0); //fbse and fbst must be between zero and one
    EXPECT_GE(fbst, 0); //fbse and fbst must be between zero and one
    EXPECT_LT(fbse, 1); //fbse and fbst must be between zero and one
    EXPECT_LT(fbst, 1); //fbse and fbst must be between zero and one
    EXPECT_DOUBLE_EQ(fbst + fbse, 1); //Must add up to one.

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();

    //TEST when fbse < bsemax
    blivelai = 0.0012, lai_param = 5, fbseExpected = 0.994018,
      fbstExpected = 0.005982036;
    EsT_partitioning(&fbse, &fbst, blivelai, lai_param);

    EXPECT_NEAR(fbse, fbseExpected, tol6); //fbse is expected to be 0.994018

    EXPECT_NEAR(fbst, fbstExpected, tol6); //fbst is expected to be 0.005982036
    EXPECT_GE(fbse, 0); //fbse and fbst must be between zero and one
    EXPECT_GE(fbst, 0); //fbse and fbst must be between zero and one
    EXPECT_LT(fbse, 1); //fbse and fbst must be between zero and one
    EXPECT_LT(fbst, 1); //fbse and fbst must be between zero and one
    EXPECT_DOUBLE_EQ(fbst + fbse, 1);  //Must add up to one.
    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }

  // TEST pot_soil_evap
  TEST(SWFlowTest, pot_soil_evap)
  {
    unsigned int i, k, nelyrs;
    double bserate = 0, totagb, Es_param_limit = 999.,
      fbse = 0.813, fbse0 = 0., petday = 0.1, petday0 = 0.,
      shift = 45, shape = 0.1, inflec = 0.25, range = 0.5;

    double swc[25], width[25], lyrEvapCo[25];
    double swc0[25] = { 0. }; // for test if swc = 0

    // Loop over tests with varying number of soil layers
    for (k = 0; k < 2; k++)
    {
      // Select number of soil layers used in test
      if (k == 0)
      {
        nelyrs = 1; // test 1: 1 soil layer
      } else if (k == 1)
      {
        nelyrs = 25; // test 2: 25 soil layers
      }

      // Setup soil layers
      create_test_soillayers(nelyrs);

      ForEachSoilLayer(i)
      {
        // copy soil layer values into arrays so that they can be passed as
        // arguments to `pot_soil_evap`
        width[i] = s->lyr[i]->width;
        lyrEvapCo[i] = s->lyr[i]->evap_coeff;

        // example: swc as mean of wilting point and field capacity
        swc[i] = (s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt) / 2.;
      }

      // Begin TEST if (totagb >= Es_param_limit)
      totagb = Es_param_limit + 1.;
      pot_soil_evap(&bserate, nelyrs, lyrEvapCo, totagb, fbse, petday, shift,
        shape, inflec, range, width, swc, Es_param_limit);

      // expect baresoil evaporation rate = 0 if totagb >= Es_param_limit
      EXPECT_DOUBLE_EQ(bserate, 0.) <<
        "pot_soil_evap != 0 if biom >= limit for " << nelyrs << " soil layers";


      // Begin TESTs if (totagb < Es_param_limit)
      totagb = Es_param_limit / 2;

      // Begin TEST if (PET = 0)
      pot_soil_evap(&bserate, nelyrs, lyrEvapCo, totagb, fbse, petday0, shift,
        shape, inflec, range, width, swc, Es_param_limit);

      // expect baresoil evaporation rate = 0 if PET = 0
      EXPECT_DOUBLE_EQ(bserate, 0.) <<
        "pot_soil_evap != 0 if PET = 0 for " << nelyrs << " soil layers";


      // Begin TEST if (potential baresoil rate = 0)
      pot_soil_evap(&bserate, nelyrs, lyrEvapCo, totagb, fbse0, petday, shift,
        shape, inflec, range, width, swc, Es_param_limit);

      // expect baresoil evaporation rate = 0 if fbse = 0
      EXPECT_DOUBLE_EQ(bserate, 0.) <<
        "pot_soil_evap != 0 if fbse = 0 for " << nelyrs << " soil layers";


      // Begin TEST if (swc = 0)
      pot_soil_evap(&bserate, nelyrs, lyrEvapCo, totagb, fbse, petday, shift,
        shape, inflec, range, width, swc0, Es_param_limit);

      // expect baresoil evaporation rate = 0 if swc = 0
      EXPECT_DOUBLE_EQ(bserate, 0.) <<
        "pot_soil_evap != 0 if swc = 0 for " << nelyrs << " soil layers";


      // Begin TEST if (totagb < Es_param_limit)
      pot_soil_evap(&bserate, nelyrs, lyrEvapCo, totagb, fbse, petday, shift,
        shape, inflec, range, width, swc, Es_param_limit);

      // expect baresoil evaporation rate > 0
      // if totagb >= Es_param_limit & swc > 0
      EXPECT_GT(bserate, 0.) <<
        "pot_soil_evap !> 0 for " << nelyrs << " soil layers";

      // expect baresoil evaporation rate <= PET
      EXPECT_LE(bserate, petday) <<
        "pot_soil_evap !<= PET for " << nelyrs << " soil layers";

      // expect baresoil evaporation rate <= potential water loss fraction
      EXPECT_LE(bserate, fbse) <<
        "pot_soil_evap !<= fbse for " << nelyrs << " soil layers";


      //Reset to previous global states.
      Reset_SOILWAT2_after_UnitTest();
    }
  }


  //TEST pot_soil_evap_bs for when nelyrs = 1 and nelyrs = MAX
  TEST(SWFlowTest, pot_soil_evap_bs)
  {
    //INPUTS
    unsigned int nelyrs,i;
    double bserate = 0, petday = 0.1, shift = 45, shape = 0.1, inflec = 0.25,
      range = 0.8;
    double ecoeff[25], width[25], swc[25];

    // Loop over tests with varying number of soil layers
    for (k = 0; k < 2; k++)
    {
      // Select number of soil layers used in test
      if (k == 0)
      {
        nelyrs = 1; // test 1: 1 soil layer
      } else if (k == 1)
      {
        nelyrs = 25; // test 2: 25 soil layers
      }

      // Setup soil layers
      create_test_soillayers(nelyrs);

      ForEachSoilLayer(i)
      {
        // copy soil layer values into arrays so that they can be passed as
        // arguments to `pot_soil_evap`
        width[i] = s->lyr[i]->width;
        ecoeff[i] = s->lyr[i]->evap_coeff;
        // example: swc as mean of wilting point and field capacity
        swc[i] = (s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt) / 2.;
      }

      //Begin TEST for bserate when nelyrs = 1
      pot_soil_evap_bs(&bserate, nelyrs, ecoeff, petday, shift, shape, inflec,
                       range, width, swc);
      if (nelyrs == 1)
      {
        EXPECT_NEAR(bserate, 0.062997815, tol6) <<
          "pot_soil_evap_bs != 0.06306041 if nelyr = 1 for " << nelyrs << " soil layesrs";
        //Reset to previous global states.
        Reset_SOILWAT2_after_UnitTest();
        // Setup soil layers for next test
      }
      if (nelyrs == 25)
      {
        EXPECT_NEAR(bserate, 0.062997200, tol6) <<
          "pot_soil_evap_bs != 0.06306041 if nelyr = 25 for " << nelyrs << " soil layesrs";
      }
    }

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }


  //Test pot_transp by manipulating biolive and biodead input variables
  TEST(SWFlowTest, pot_transp)
  {
    //INPUTS
    double bstrate = 0, swpavg = 0.8, biolive = -0.8, biodead = 0.2, fbst = 0.8, petday = 0.1,
      swp_shift = 45, swp_shape = 0.1, swp_inflec = 0.25, swp_range = 0.3, shade_scale = 1.1,
        shade_deadmax = 0.9, shade_xinflex = 0.4, shade_slope = 0.9, shade_yinflex = 0.3,
          shade_range = 0.8, co2_wue_multiplier = 2.1;

    //Begin TEST for if biolive < 0
    pot_transp(&bstrate, swpavg, biolive, biodead, fbst, petday, swp_shift, swp_shape,
      swp_inflec, swp_range, shade_scale, shade_deadmax, shade_xinflex, shade_slope,
        shade_yinflex, shade_range, co2_wue_multiplier);

    //INPUTS for expected outputs
    double bstrateExpected = 0.06596299;

    EXPECT_DOUBLE_EQ(bstrate, 0); //bstrate = 0 if biolove < 0

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();

    //Begin TEST for if biolive > 0
    biolive = 0.8;
    pot_transp(&bstrate, swpavg, biolive, biodead, fbst, petday, swp_shift, swp_shape,
      swp_inflec, swp_range, shade_scale, shade_deadmax, shade_xinflex, shade_slope,
        shade_yinflex, shade_range, co2_wue_multiplier);

    EXPECT_NEAR(bstrate, bstrateExpected, tol6); //For this test local variable shadeaf = 1, affecting bstrate
                                   //bstrate is expected to be 0.06596299

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();

    //Begin TEST for if biodead > shade_deadmax
    biodead = 0.95, bstrateExpected = 0.0659629;
    pot_transp(&bstrate, swpavg, biolive, biodead, fbst, petday, swp_shift, swp_shape,
      swp_inflec, swp_range, shade_scale, shade_deadmax, shade_xinflex, shade_slope,
        shade_yinflex, shade_range, co2_wue_multiplier);

    EXPECT_NEAR(bstrate, bstrateExpected, tol6); //bstrate is expected to be 0.06564905

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();

    //Begin TEST for if biodead < shade_deadmax
    biodead = 0.2, bstrateExpected = 0.0659629;
    pot_transp(&bstrate, swpavg, biolive, biodead, fbst, petday, swp_shift, swp_shape,
      swp_inflec, swp_range, shade_scale, shade_deadmax, shade_xinflex, shade_slope,
        shade_yinflex, shade_range, co2_wue_multiplier);

    EXPECT_NEAR(bstrate, bstrateExpected, tol6); //For this test local variable shadeaf = 1, affecting bstrate
                                   //bstrate is expected to be 0.06596299

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }

  //Test result for watrate by manipulating variable petday
  TEST(SWFlowTest, watrate)
  {
    //INPUTS
    double swp = 0.8, petday = 0.1, shift = 45, shape = 0.1, inflec = 0.25, range = 0.8;

    //Begin TEST for if petday < .2
    double watExpected = 0.630365;
    double wat = watrate(swp, petday, shift, shape, inflec, range);

    EXPECT_NEAR(wat, watExpected, tol6); //When petday = 0.1, watrate = 0.630365
    EXPECT_LE(wat, 1); //watrate must be between 0 & 1
    EXPECT_GE(wat, 0); //watrate must be between 0 & 1

    //Begin TEST for if 0.2 < petday < .4
    petday = 0.3, watExpected = 0.6298786;
    wat = watrate(swp, petday, shift, shape, inflec, range);

    EXPECT_NEAR(wat, watExpected, tol6); //When petday = 0.3, watrate = 0.6298786
    EXPECT_LE(wat, 1); //watrate must be between 0 & 1
    EXPECT_GE(wat, 0); //watrate must be between 0 & 1

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();

    //Begin TEST for if 0.4 < petday < .6
    petday = 0.5, watExpected = 0.6285504;
    wat = watrate(swp, petday, shift, shape, inflec, range);

    EXPECT_NEAR(wat, watExpected, tol6); //When petrate = 0.5, watrate = 0.6285504
    EXPECT_LE(wat, 1); //watrate must be between 0 & 1
    EXPECT_GE(wat, 0); //watrate must be between 0 & 1

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();

    //Begin TEST for if 0.6 < petday < 1
    petday = 0.8, watExpected = 0.627666;
    wat = watrate(swp, petday, shift, shape, inflec, range);

    EXPECT_NEAR(wat, watExpected, tol6); //When petday = 0.8, watrate = 0.627666
    EXPECT_LE(wat, 1); //watrate must be between 0 & 1
    EXPECT_GE(wat, 0); //watrate must be between 0 & 1

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }

  //Test evap_fromSurface by manipulating water_pool and evap_rate variables
  TEST(SWFlowTest, evap_fromSurface)
  {
    //INPUTS
    double water_pool = 1, evap_rate = 0.33, aet = 0.53;

    //Begin TEST for when water_pool > evap_rate
    double aetExpected = 0.86, evapExpected = 0.33, waterExpected = 0.67;
    evap_fromSurface(&water_pool, &evap_rate, &aet);

    EXPECT_NEAR(aet, aetExpected, tol6); //Variable aet is expected to be 0.86 with current inputs
    EXPECT_GE(aet, 0); //aet is never negative

    EXPECT_NEAR(evap_rate, evapExpected, tol6); //Variable evap_rate is expected to be 0.33 with current inputs
    EXPECT_GE(evap_rate, 0); //evap_rate is never negative

    EXPECT_NEAR(water_pool, waterExpected, tol6); //Variable water_pool is expected to be 0.67 with current inputs
    EXPECT_GE(water_pool, 0); //water_pool is never negative

    //Begin TEST for when water_pool < evap_rate
    water_pool = 0.1, evap_rate = 0.67, aet = 0.78, aetExpected = 0.88, evapExpected = 0.1;
    evap_fromSurface(&water_pool, &evap_rate, &aet);

    EXPECT_NEAR(aet, aetExpected, tol6); //Variable aet is expected to be 0.88 with current inputs
    EXPECT_GE(aet, 0); //aet is never negative

    EXPECT_NEAR(evap_rate, evapExpected, tol6); //Variable evap_rate is expected to be 0.1 with current inputs
    EXPECT_GE(evap_rate, 0); //evap_rate is never negative

    EXPECT_DOUBLE_EQ(water_pool, 0); //Variable water_pool is expected to be 0 when water_pool < evap_rate
    EXPECT_GE(water_pool, 0); //water_pool is never negative

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }

  //Test remove_from_soil when nlyrs = 1 and when nlyrs = MAX
  TEST(SWFlowTest, remove_from_soil)
  {
    // INPUTS
    unsigned int nlyrs, i;
    double aet_init = 0.33, aet, rate = 0.62;
    double swc_init[MAX_LAYERS], swc[MAX_LAYERS], swcmin[MAX_LAYERS];
    double qty[MAX_LAYERS], qty_sum;
    double coeff[MAX_LAYERS], coeffZero[MAX_LAYERS] = {0.};

    // Expected outcomes
    double swcExpected[MAX_LAYERS], qtyExpected[MAX_LAYERS], aetExpected;
    double swcExpected_1L[1] = {0.335102002};
    double qtyExpected_1L[1] = {0.490786736};
    double swcExpected_MAXL[MAX_LAYERS] = {
      0.797272468, 0.188079794, 0.959050850, 0.225652538, 0.212587913,
      1.830213267, 0.147813558, 0.147813558, 0.411441988, 0.900635851,
      2.171560691, 0.196976313, 0.212587913, 1.830213267, 0.147813558,
      0.147813558, 0.117792659, 0.165094114, 0.187453675, 1.114615515,
      2.308462927, 2.292794374, 1.658226136, 3.336462333, 6.688236569};
    double qtyExpected_MAXL[MAX_LAYERS] = {
      0.028616270, 0.018791319, 0.033002659, 0.032433487, 0.020287088,
      0.020111162, 0.020010061, 0.020010061, 0.029032004, 0.018791319,
      0.033002659, 0.032433487, 0.020287088, 0.020111162, 0.020010061,
      0.020010061, 0.029032004, 0.018791319, 0.033002659, 0.032433487,
      0.020287088, 0.020111162, 0.020010061, 0.020010061, 0.039382201};


    // Loop over tests with varying number of soil layers
    for (k = 0; k < 2; k++)
    {
      // Select number of soil layers used in test
      if (k == 0)
      {
        nlyrs = 1; // test 1: 1 soil layer
      } else if (k == 1)
      {
        nlyrs = MAX_LAYERS; // test 2: MAX_LAYERS soil layers
      }

      // Setup soil layers
      create_test_soillayers(nlyrs);

      ForEachSoilLayer(i)
      {
        // copy soil layer values into arrays so that they can be passed as
        // arguments to `pot_soil_evap`
        swcmin[i] = s->lyr[i]->swcBulk_min;
        // example: swc as mean of wilting point and field capacity
        swc_init[i] = (s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt) / 2.;
        // example: coeff as shrub trco
        coeff[i] = s->lyr[i]->transp_coeff[SW_SHRUB];
      }

      // Begin TEST: coeff[i] == 0 --> no water to extract
      // Re-set inputs
      aet = aet_init;
      ForEachSoilLayer(i)
      {
        qty[i] = 0.;
        swc[i] = swc_init[i];
      }

      // Expected output: identical to inputs
      aetExpected = aet_init;
      ForEachSoilLayer(i)
      {
        swcExpected[i] = swc[i];
        qtyExpected[i] = 0.;
      }

      // Call function: coeffZero used instead of coeff
      remove_from_soil(swc, qty, &aet, nlyrs, coeffZero, rate, swcmin);

      // Check expectation of no change from original values
      qty_sum = 0.;
      for (i = 0; i < nlyrs; i++)
      {
        EXPECT_NEAR(qty[i], qtyExpected[i], tol6) <<
          "remove_from_soil: qty != qty for layer " << 1 + i << " out of "
          << nlyrs << " soil layers";
        EXPECT_NEAR(swc[i], swcExpected[i], tol6) <<
          "remove_from_soil: swc != swc for layer " << 1 + i << " out of "
          << nlyrs << " soil layers";
          qty_sum += qty[i];
      }
      EXPECT_DOUBLE_EQ(aet, aetExpected) <<
          "remove_from_soil(no coeff): aet != aet for " << nlyrs <<
          " soil layers";
      EXPECT_DOUBLE_EQ(qty_sum, 0.) <<
          "remove_from_soil: sum(qty) != 0 for " << nlyrs << " soil layers";

      //Begin TEST: frozen[i] --> no water to extract
      //Re-set inputs and set soil layers as frozen
      aet = aet_init;
      ForEachSoilLayer(i)
      {
        st->lyrFrozen[i] = swTRUE;
        qty[i] = 0.;
        swc[i] = swc_init[i];
      }

      // Call function: switch to coeff input
      remove_from_soil(swc, qty, &aet, nlyrs, coeff, rate, swcmin);

      // Check expectation of no change from original values
      qty_sum = 0.;
      for (i = 0; i < nlyrs; i++)
      {
        EXPECT_NEAR(qty[i], qtyExpected[i], tol6) <<
          "remove_from_soil(frozen): qty != qty for layer " << 1 + i <<
          " out of " << nlyrs << " soil layers";
        EXPECT_NEAR(swc[i], swcExpected[i], tol6) <<
          "remove_from_soil(frozen): swc != swc for layer " << 1 + i <<
          " out of " << nlyrs << " soil layers";
        qty_sum += qty[i];
      }
      EXPECT_DOUBLE_EQ(aet, aetExpected) <<
          "remove_from_soil(frozen): aet != aet for " << nlyrs <<
          " soil layers";
      EXPECT_DOUBLE_EQ(qty_sum, 0.) <<
          "remove_from_soil: sum(qty) != 0 for " << nlyrs << " soil layers";


      // TEST if some coeff[i] > 0
      // Re-set inputs
      aet = aet_init;
      ForEachSoilLayer(i)
      {
        st->lyrFrozen[i] = swFALSE;
        qty[i] = 0.;
        swc[i] = swc_init[i];
      }

      // Expected output
      aetExpected = aet_init;
      ForEachSoilLayer(i)
      {
        if (k == 0)
        {
          swcExpected[i] = swcExpected_1L[i];
          qtyExpected[i] = qtyExpected_1L[i];

        } else if (k == 1)
        {
          swcExpected[i] = swcExpected_MAXL[i];
          qtyExpected[i] = qtyExpected_MAXL[i];
        }

        aetExpected += qtyExpected[i];
      }

      // Call function: switch to coeff input
      remove_from_soil(swc, qty, &aet, nlyrs, coeff, rate, swcmin);

      // Check values of qty[] and swc[] against array of expected values
      qty_sum = 0.;
      for (i = 0; i < nlyrs; i++)
      {
        EXPECT_NEAR(qty[i], qtyExpected[i], tol6) <<
          "remove_from_soil: qty != expected for layer " << 1 + i << " out of "
          << nlyrs << " soil layers";
        EXPECT_NEAR(swc[i], swcExpected[i], tol6) <<
          "remove_from_soil: swc != expected for layer " << 1 + i << " out of "
          << nlyrs << " soil layers";
        qty_sum += qty[i];
      }
      EXPECT_NEAR(aet, aetExpected, tol6) <<
          "remove_from_soil: aet != expected for " << nlyrs << " soil layers";
      EXPECT_GT(qty_sum, 0.) <<
          "remove_from_soil: sum(qty) !> 0 for " << nlyrs << " soil layers";
      // detailed message due to failure on appveyor-ci (but not travis-ci):
      // fails with "Expected: (qty_sum) <= (rate), actual: 0.62 vs 0.62"
      // --> it almost appears as if `EXPECT_LE` is behaving as if `EXPECT_LT`
      // on appveyor-ci
      // --> hack for this case on appveyor-ci: add `tol9` to `rate`
      EXPECT_LE(qty_sum, rate + tol9) << std::setprecision(12) << std::fixed <<
          "remove_from_soil: sum(qty)=" << qty_sum <<
          " !<= rate=" << rate <<
          " for " << nlyrs << " soil layers";
    }

    // Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }


  //Test when nlyrs = 1 and 25 for outputs; swc, drain, drainout, standing water
  TEST(SWFlowTest, infiltrate_water_low)
  {
    //INPUTS
    unsigned int nlyrs, i, k;
    double drainout = 0.1, sdrainpar = 0.6, sdraindpth = 6, standingWater = 0.;
    double swc[MAX_LAYERS], drain[MAX_LAYERS], swcfc[MAX_LAYERS];
    double width[MAX_LAYERS], swcmin[MAX_LAYERS], swcsat[MAX_LAYERS];
    double impermeability[MAX_LAYERS] = {0.};

    // Loop over tests with varying number of soil layers
    for (k = 0; k < 2; k++)
    {
      // Select number of soil layers used in test
      if (k == 0)
      {
        nlyrs = 1; // test 1: 1 soil layer
      } else if (k == 1)
      {
        nlyrs = MAX_LAYERS; // test 2: MAX_LAYERS soil layers
      }

      if(k == 1) //nlyrs = MAX_LAYERS
      {

        //Setup soil layers
        create_test_soillayers(nlyrs);
        drainout = 0.1, standingWater = 0;
        ForEachSoilLayer(i)
        {
          // copy soil layer values into arrays so that they can be passed as
          // example: swc as mean of wilting point and field capacity
          swc[i] = (s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt)
            / 2.;
          drain[i] = 1.;
          swcfc[i] = s->lyr[i]->swcBulk_fieldcap;
          width[i] = s->lyr[i]->width;
          swcmin[i] = s->lyr[i]->swcBulk_min;
          swcsat[i] = s->lyr[i]->swcBulk_saturated;
        }

        //INPUTS for expected outputs
        double swcExpected0[MAX_LAYERS] = {
          0.335102, 0.084524, 0.369557, 0.092513, 0.072265,
          0.578848, 0.079652, 0.079652, 0.179332, 0.365506,
          0.749107, 0.073229, 0.072265, 0.578848, 0.079652,
          0.079652, 0.059777, 0.073101, 0.074911, 0.366146,
          0.722649, 0.723560, 0.796521, 1.593042, 3.223746};
        double drainExpected0[MAX_LAYERS];
        double drainoutExpected = 0.1, standingWaterExpected = 0.;
        ForEachSoilLayer(i)
        {
          drainExpected0[i] = 1.;
        }
        //Begin TEST for if swc[i] <= swcmin[i]
        //Inputs for swc and swcmin have been swapped
        ForEachSoilLayer(i)
        {
          swc[i] = s->lyr[i]->swcBulk_min;
          swcmin[i] = (s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt)
            / 2.;
          drain[i] = 1.;
        }

        infiltrate_water_low(swc, drain, &drainout, nlyrs, sdrainpar,
          sdraindpth, swcfc, width, swcmin, swcsat, impermeability,
          &standingWater);
        //drainout expected to be 0.1
        EXPECT_NEAR(drainoutExpected, drainout, tol6);
        //standingWater expected to be 0.
        EXPECT_NEAR(standingWaterExpected, standingWater, tol6);

        ForEachSoilLayer(i)
        {
          EXPECT_NEAR(swc[i], swcExpected0[i], tol6) <<
            "infiltrate_water_low: swc != expected for layer " << 1 + i;
          EXPECT_NEAR(drain[i], drainExpected0[i], tol6) <<
            "infiltrate_water_low: drain != expected for layer " << 1 + i;
        }

        //Reset to previous global states.
        Reset_SOILWAT2_after_UnitTest();

        //Begin TEST for if swc[i] > swcmin[i]
        //Setup soil layers
        create_test_soillayers(nlyrs);
        //Reset INPUTS; swc[i] > swcmin[i]
        ForEachSoilLayer(i)
        {
          swc[i] = (s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt)
            / 2.;
          drain[i] = 1.;
          swcfc[i] = s->lyr[i]->swcBulk_fieldcap;
          width[i] = s->lyr[i]->width;
          swcmin[i] = s->lyr[i]->swcBulk_min;
          swcsat[i] = s->lyr[i]->swcBulk_saturated;
        }
        drainout = 0.1, standingWater = 0;

        //Reset expected outcomes
        double swcExpected1[MAX_LAYERS] = {
          0.399392, 0.084524, 0.940897, 0.258086, 0.232875,
          1.850324, 0.167824, 0.167824, 0.440474, 0.919427,
          2.204563, 0.229410, 0.232875, 1.850324, 0.167824,
          0.167824, 0.146825, 0.183885, 0.220456, 1.147049,
          2.328750, 2.312906, 1.678236, 3.400368, 6.776540};
        double drainExpected1[MAX_LAYERS] = {
          1.426497, 1.548844, 1.600000, 1.600000, 1.600000,
          1.600000, 1.600000, 1.600000, 1.600000, 1.600000,
          1.600000, 1.600000, 1.600000, 1.600000, 1.600000,
          1.600000, 1.600000, 1.600000, 1.600000, 1.600000,
          1.600000, 1.600000, 1.600000, 1.556104, 1.507181};
        standingWaterExpected = 0, drainoutExpected = 0.607181;

        infiltrate_water_low(swc, drain, &drainout, nlyrs, sdrainpar,
          sdraindpth, swcfc, width, swcmin, swcsat, impermeability,
          &standingWater);

        //drainout is expected to be 0.1
        EXPECT_NEAR(drainoutExpected, drainout, tol6);
        //standingWater is expected to be 0.
        EXPECT_NEAR(standingWater, standingWaterExpected, tol6);

        ForEachSoilLayer(i)
        {
          EXPECT_NEAR(swc[i], swcExpected1[i], tol6) <<
            "infiltrate_water_low: swc != expected for layer " << 1 + i;
          EXPECT_NEAR(drain[i], drainExpected1[i], tol6) <<
            "infiltrate_water_low: drain != expected for layer " << 1 + i;
        }

        //Reset to previous global states.
        Reset_SOILWAT2_after_UnitTest();

        //Setup soil layers
        create_test_soillayers(nlyrs);

        //Begin TEST for if swc[j] > swcsat[j]
        //Reset INPUTS; swc[j] > swcsat[j]
        //swc and swcsat have been swapped
        ForEachSoilLayer(i)
        {
          swcsat[i] = ((s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt)
            / 2.);
          swc[i] = s->lyr[i]->swcBulk_saturated;
          drain[i] = 1;
        }
        standingWater = 0, drainout = 0.1;

        //INPUTS for expected outputs
        double swcExpected2[MAX_LAYERS] = {
          0.825889, 0.206871, 0.992054, 0.258086, 0.232875,
          1.850324, 0.167824, 0.167824, 0.440474, 0.919427,
          2.204563, 0.229410, 0.232875, 1.850324, 0.167824,
          0.167824, 0.146825, 0.183885, 0.220456, 1.147049,
          2.328750, 2.312906, 1.678236, 3.356472, 6.727619};
        double drainExpected2[MAX_LAYERS] = {
          -19.455442, -19.300536, -18.690795, -18.534689, -18.393543,
          -17.261477, -17.113211, -16.964945, -16.483618, -15.795148,
          -14.440166, -14.301405, -14.160259, -13.028194, -12.879927,
          -12.731661, -12.571219, -12.433525, -12.298027, -11.604222,
          -10.192758, -8.777677, -7.295012, -4.329682, 1.6};
        standingWaterExpected = 21.35793, drainoutExpected = 0.7;

        infiltrate_water_low(swc, drain, &drainout, nlyrs, sdrainpar,
          sdraindpth, swcfc, width, swcmin, swcsat, impermeability,
          &standingWater);

        //drainout is expected to be 0.1
        EXPECT_NEAR(drainoutExpected, drainout, tol6);
        //standingWater is expected to be 0.
        EXPECT_NEAR(standingWater, standingWaterExpected, tol6);

        ForEachSoilLayer(i)
        {
        EXPECT_NEAR(swc[i], swcExpected2[i], tol6) <<
          "infiltrate_water_low: swc != expected for layer " << 1 + i;
        EXPECT_NEAR(drain[i], drainExpected2[i], tol6) <<
          "infiltrate_water_low: drain != expected for layer " << 1 + i;
        }

        //Reset to previous global states.
        Reset_SOILWAT2_after_UnitTest();

        //Setup soil layers
        create_test_soillayers(nlyrs);

        //Begin TEST for if swc[j] <= swcsat[j]
        //INPUTS
        //Reset INPUTS; swc[j] <= swcsat[j]
        //swc and swcsat have been reset
        ForEachSoilLayer(i)
        {
          swc[i] = (s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt)
            / 2.;
          drain[i] = 1.;
          swcfc[i] = s->lyr[i]->swcBulk_fieldcap;
          width[i] = s->lyr[i]->width;
          swcmin[i] = s->lyr[i]->swcBulk_min;
          swcsat[i] = s->lyr[i]->swcBulk_saturated;
        }
        standingWater = 0, drainout = 0.1;

        //INPUTS for expected outputs
        double swcExpected3[MAX_LAYERS] = {
          0.399392, 0.084524, 0.940896, 0.258086, 0.232875,
          1.850324, 0.167824, 0.167824, 0.440474, 0.919427,
          2.204563, 0.229410, 0.232875, 1.850324, 0.167824,
          0.167824, 0.146825, 0.183885, 0.220456, 1.147049,
          2.328750, 2.312906, 1.678236, 3.400368, 6.776541};
        double drainExpected3[MAX_LAYERS] = {
          1.426497, 1.548844, 1.600000, 1.600000, 1.600000,
          1.600000, 1.600000, 1.600000, 1.600000, 1.600000,
          1.600000, 1.600000, 1.600000, 1.600000, 1.600000,
          1.600000, 1.600000, 1.600000, 1.600000, 1.600000,
          1.600000, 1.600000, 1.600000, 1.556104, 1.507182};
        standingWaterExpected = 0, drainoutExpected = 0.607181;

        infiltrate_water_low(swc, drain, &drainout, nlyrs, sdrainpar,
          sdraindpth, swcfc, width, swcmin, swcsat, impermeability,
          &standingWater);

        //drainout is expected to be 0.607181
        EXPECT_NEAR(drainoutExpected, drainout, tol6);
        //standingWater is expected to be 0.
        EXPECT_NEAR(standingWater, standingWaterExpected, tol6);

        ForEachSoilLayer(i)
        {
          EXPECT_NEAR(swc[i], swcExpected3[i], tol6) <<
            "infiltrate_water_low: swc != expected for layer " << 1 + i;
          EXPECT_NEAR(drain[i], drainExpected3[i], tol6) <<
            "infiltrate_water_low: drain != expected for layer " << 1 + i;
        }

      }
      //Reset to previous global states.
      Reset_SOILWAT2_after_UnitTest();

      //Setup soil layers
      create_test_soillayers(nlyrs);
      if(k == 0) //nlyrs = 1
      {
        //INPUTS for one layer
        ForEachSoilLayer(i)
        {
          // copy soil layer values into arrays so that they can be passed as
          // example: swc as mean of wilting point and field capacity
          swc[i] = (s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt)
            / 2.;
          drain[i] = 1.;
          swcfc[i] = s->lyr[i]->swcBulk_fieldcap;
          width[i] = s->lyr[i]->width;
          swcmin[i] = s->lyr[i]->swcBulk_min;
          swcsat[i] = s->lyr[i]->swcBulk_saturated;
        }
        //INPUTS for expected outputs
        double swcExpected_1[1] = {0.335102};
        double drainExpected_1[1] = {1.};
        double drainoutExpected = 0.1, standingWaterExpected = 0;

        //Begin TEST for if swc[i] <= swcmin[i]
        //Inputs for swc and swcmin have been swapped
        ForEachSoilLayer(i)
        {
          swc[i] = s->lyr[i]->swcBulk_min;
          swcmin[i] = (s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt)
            / 2.;
        }
        infiltrate_water_low(swc, drain, &drainout, nlyrs, sdrainpar,
          sdraindpth, swcfc, width, swcmin, swcsat, impermeability,
          &standingWater);

        //drainout is expected to be 0.1
        EXPECT_NEAR(drainoutExpected, drainout, tol6);
        //standingWater is expected to be 0.
        EXPECT_NEAR(standingWater, standingWaterExpected, tol6);

        //swc is expected to match 0.335102
        EXPECT_NEAR(swc[0], swcExpected_1[0], tol6);
        //drain is expected to match 1.
        EXPECT_DOUBLE_EQ(drain[0], drainExpected_1[0]);

        //Reset to previous global states.
        Reset_SOILWAT2_after_UnitTest();

        //Setup soil layers
        create_test_soillayers(nlyrs);

        //Begin TEST for if swc[i] > swcmin[i]
        standingWater = 0, drainout = 0.1;
        ForEachSoilLayer(i)
        {
          swc[i] = (s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt)
            / 2.;
          swcmin[i] = s->lyr[i]->swcBulk_min;
          drain[i] = 1.;
        }
        double swcExpected2_1[1] = {0.399392};
        double drainExpected2_1[1] = {1.426497};
        standingWaterExpected = 0., drainoutExpected = 0.526497;

        infiltrate_water_low(swc, drain, &drainout, nlyrs, sdrainpar,
          sdraindpth, swcfc, width, swcmin, swcsat, impermeability,
          &standingWater);

        //drainout is expected to be 0.526497
        EXPECT_NEAR(drainoutExpected, drainout, tol6);
        //standingWater is expected to be 0.
        EXPECT_NEAR(standingWater, standingWaterExpected, tol6);

        //swc is expected to match 0.399392
        EXPECT_NEAR(swc[0], swcExpected2_1[0], tol6);
        //drain is expected to match 1.426497
        EXPECT_NEAR(drain[0], drainExpected2_1[0], tol6);

        //Reset to previous global states.
        Reset_SOILWAT2_after_UnitTest();

        //Setup soil layers
        create_test_soillayers(nlyrs);

        //Begin TEST for if lyrFrozen == true
        //INPUTS for expected outputs
        double swcExpected3_1[1] = {0.821624};
        double drainoutExpectedf = 0.104265;
        //Set lyrFrozen
        ForEachSoilLayer(i)
        {
          st->lyrFrozen[i] = swTRUE;
        }

        infiltrate_water_low(swc, drain, &drainout, nlyrs, sdrainpar,
          sdraindpth, swcfc, width, swcmin, swcsat, impermeability,
          &standingWater);

        if (st->lyrFrozen[i] == swTRUE)
        {
          //standingWater is expected to be 0.
          EXPECT_NEAR(standingWater, standingWaterExpected, tol6);
          //drainout is expected to be 0.104265
          EXPECT_NEAR(drainoutExpectedf, drainout, tol6);
          //swc is expected to match 0.821624
          EXPECT_NEAR(swc[0], swcExpected3_1[0], tol6);
          //drain is expected to match 1.426497
          EXPECT_NEAR(drain[0], drainExpected2_1[0], tol6);
        }
        //Reset to previous global states.
        Reset_SOILWAT2_after_UnitTest();
        //Setup soil layers
        create_test_soillayers(nlyrs);

        //Begin TEST for if lyrFrozen == false
        ForEachSoilLayer(i)
        {
          st->lyrFrozen[i] = swFALSE;
          swc[i] = (s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt)
            / 2.;
          drain[i] = 1.;
          swcfc[i] = s->lyr[i]->swcBulk_fieldcap;
          width[i] = s->lyr[i]->width;
          swcmin[i] = s->lyr[i]->swcBulk_min;
          swcsat[i] = s->lyr[i]->swcBulk_saturated;
        }

        //Reset to previous global states.
        Reset_SOILWAT2_after_UnitTest();

        //Setup soil layers
        create_test_soillayers(nlyrs);

        //Begin TEST for if swc[j] > swcsat[j]

        //INPUTS
        //swc and swcsat have been swapped
        standingWater = 0, drainout = 0.1;
        ForEachSoilLayer(i)
        {
          swcsat[i] = (s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt)
            / 2.;
          drain[i] = 1.;
          swcfc[i] = s->lyr[i]->swcBulk_fieldcap;
          width[i] = s->lyr[i]->width;
          swcmin[i] = s->lyr[i]->swcBulk_min;
          swc[i] = s->lyr[i]->swcBulk_saturated;
        }

        //INPUTS for expected outputs
        double swcExpected4_1[1] = {0.825889};
        double drainExpected4_1[1] = {1.6};
        standingWaterExpected = 0.302488, drainoutExpected = 0.7;

        infiltrate_water_low(swc, drain, &drainout, nlyrs, sdrainpar,
          sdraindpth, swcfc, width, swcmin, swcsat, impermeability,
          &standingWater);

        //drainout is expected to be 0.7
        EXPECT_NEAR(drainout, drainoutExpected, tol6);
        //standingWater expected to be 0.302488
        EXPECT_NEAR(standingWater, standingWaterExpected, tol6);
        //swc is expected to match 0.825889
        EXPECT_NEAR(swc[0], swcExpected4_1[0], tol6);
        //drain is expected to match 1.6
        EXPECT_NEAR(drain[0], drainExpected4_1[0], tol6);

        //Reset to previous global states.
        Reset_SOILWAT2_after_UnitTest();

        //Setup soil layers
        create_test_soillayers(nlyrs);

        //Begin TEST for if swc[j] <= swcsat[j]
        //Reset INPUTS; swc[j] <= swcsat[j]
        //swc and swcsat have been reset
        standingWater = 0, drainout = 0.1;
        ForEachSoilLayer(i)
        {
          swc[i] = (s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt)
            / 2.;
          drain[i] = 1.;
          swcfc[i] = s->lyr[i]->swcBulk_fieldcap;
          width[i] = s->lyr[i]->width;
          swcmin[i] = s->lyr[i]->swcBulk_min;
          swcsat[i] = s->lyr[i]->swcBulk_saturated;
        }

        //INPUTS for expected outputs
        double swcExpected5_1[1] = {0.399392};
        double drainExpected5_1[1] = {1.426497};
        standingWaterExpected = 0., drainoutExpected = 0.526497;

        infiltrate_water_low(swc, drain, &drainout, nlyrs, sdrainpar,
          sdraindpth, swcfc, width, swcmin, swcsat, impermeability,
          &standingWater);

        //drainout is expected to be 0.526497
        EXPECT_NEAR(drainout, drainoutExpected, tol6);
        //standingWater expected to be 0.
        EXPECT_NEAR(standingWater, standingWaterExpected, tol6);
        //swc is expected to be 0.399392
        EXPECT_NEAR(swc[0], swcExpected5_1[0], tol6);
        //drain is expected to be 1.426497
        EXPECT_NEAR(drain[0], drainExpected5_1[0], tol6);

      }
      //Reset to previous global states.
      Reset_SOILWAT2_after_UnitTest();
    }
  }

  //TEST for hydraulic_redistribution when nlyrs = MAX_LAYERS and nlyrs = 1
  TEST(SWFlowTest, hydraulic_redistribution)
  {
    //INPUTS
    unsigned int nlyrs, i, k;
    double maxCondroot = -0.2328, swp50 = 1.2e12, shapeCond = 1, scale = 0.3;
    double swc[MAX_LAYERS], swcwp[MAX_LAYERS], lyrRootCo[MAX_LAYERS],
      hydred[MAX_LAYERS] = {0.};

    // Loop over tests with varying number of soil layers
    for (k = 0; k < 2; k++)
    {
      // Select number of soil layers used in test
      if (k == 0)
      {
        nlyrs = 1; // test 1: 1 soil layer
      } else if (k == 1)
      {
        nlyrs = MAX_LAYERS; // test 2: MAX_LAYERS soil layers
      }

      if(k == 1) //nlyrs = MAX_LAYERS
      {
        // Setup soil layers
        create_test_soillayers(nlyrs);
        ForEachSoilLayer(i)
        {
          swc[i] = (s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt)
            / 2.;
          swcwp[i] = s->lyr[i]->swcBulk_wiltpt;
          lyrRootCo[i] =  s->lyr[i]->transp_coeff[SW_SHRUB];
          st->lyrFrozen[i] = swFALSE;
        }

        //Begin TEST for if swp[i] < swpwp[i] OR swp[j] < swpwp[j] AND lyrFrozen == false;
        //j = i + 1
        //INPUTS for expected outcomes
        double swcExpected0[25] = {
          0.8258890, 0.2068147, 0.9921274, 0.2581945, 0.2329534,
          1.8504017, 0.1677781, 0.1677781, 0.4402285, 0.9193707,
          2.2046364, 0.2295185, 0.2329534, 1.8504017, 0.1677781,
          0.1677781, 0.1465795, 0.1838287, 0.2205295, 1.1471575,
          2.3288284, 2.3129837, 1.6781901, 3.3564261, 6.7275403};
        double hydredExpected0[25] = {
          0.000000e+00, -5.630918e-05, 7.341111e-05, 1.085271e-04, 7.844259e-05,
          7.767014e-05, -4.592553e-05, -4.592553e-05, -2.455028e-04, -5.631293e-05,
          7.342950e-05, 1.084979e-04, 7.844259e-05, 7.767014e-05, -4.592553e-05,
          -4.592553e-05, -2.455263e-04, -5.629044e-05, 7.346768e-05, 1.085242e-04,
          7.844259e-05, 7.766175e-05, -4.589769e-05, -4.589769e-05, -7.874827e-05};

        hydraulic_redistribution(swc, swcwp, lyrRootCo, hydred, nlyrs,
          maxCondroot, swp50, shapeCond, scale);

        EXPECT_DOUBLE_EQ(hydred[0], 0); //no hydred in top layer
        ForEachSoilLayer(i)
        {
          EXPECT_NEAR(swc[i], swcExpected0[i], tol3) <<
            "hydraulic_redistribution: swc != swcExpected0 for layer " << 1 + i << " out of "
            << nlyrs << " soil layers";
          EXPECT_NEAR(hydred[i], hydredExpected0[i], tol3) <<
            "hydraulic_redistribution: hydred != hydredExpected0 for layer "
            << 1 + i << " out of "<< nlyrs << " soil layers";
        }

        //Reset to previous global states.
        Reset_SOILWAT2_after_UnitTest();
        // Setup soil layers
        create_test_soillayers(nlyrs);
        //Reset inputs, swcwp has been altered
        ForEachSoilLayer(i)
        {
          swc[i] = (s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt)
            / 2.;
          swcwp[i] = (s->lyr[i]->swcBulk_wiltpt) / 4;
          lyrRootCo[i] =  s->lyr[i]->transp_coeff[SW_SHRUB];
          st->lyrFrozen[i] = swFALSE;
          hydred[i] = 0.;
        }
        //Begin TEST for if else ^^
        //INPUTS for expected outputs.
        double swcExpected1[25] = {
          0.8258890, 0.2068147, 0.9921274, 0.2581945, 0.2329534,
          1.8504017, 0.1677781, 0.1677781, 0.4402285, 0.9193707,
          2.2046364, 0.2295185, 0.2329534, 1.8504017, 0.1677781,
          0.1677781, 0.1465795, 0.1838287, 0.2205295, 1.1471575,
          2.3288284, 2.3129837, 1.6781901, 3.3564261, 6.7275403};
        double hydredExpected1[25] = {
          0.000000e+00, -5.630918e-05, 7.341111e-05, 1.085271e-04, 7.844259e-05,
          7.767014e-05, -4.592553e-05, -4.592553e-05, -2.455028e-04, -5.631293e-05,
          7.342950e-05, 1.084979e-04, 7.844259e-05, 7.767014e-05, -4.592553e-05,
          -4.592553e-05, -2.455263e-04, -5.629044e-05, 7.346768e-05, 1.085242e-04,
          7.844259e-05, 7.766175e-05, -4.589769e-05, -4.589769e-05, -7.874827e-05};

        hydraulic_redistribution(swc, swcwp, lyrRootCo, hydred, nlyrs,
          maxCondroot, swp50, shapeCond, scale);

        EXPECT_DOUBLE_EQ(hydred[0], 0); //no hydred in top layer
        ForEachSoilLayer(i)
        {
          EXPECT_NEAR(swc[i], swcExpected1[i], tol3) <<
            "hydraulic_redistribution: swc != swcExpected1 for layer " << 1 + i <<
            " out of "<< nlyrs << " soil layers";
          EXPECT_NEAR(hydred[i], hydredExpected1[i], tol3) <<
            "hydraulic_redistribution: hydred != hydredExpected1 for layer "
            << 1 + i << " out of "<< nlyrs << " soil layers";
        }
      }
      //Reset to previous global states.
      Reset_SOILWAT2_after_UnitTest();
      // Setup soil layers
      create_test_soillayers(nlyrs);
      if(k == 0) //nlyrs = 1
      {
        swc[0] = (s->lyr[0]->swcBulk_fieldcap + s->lyr[0]->swcBulk_wiltpt)
          / 2.;
        swcwp[0] = (s->lyr[0]->swcBulk_wiltpt);
        lyrRootCo[0] =  s->lyr[0]->transp_coeff[SW_SHRUB];
        st->lyrFrozen[0] = swFALSE;
        hydred[0] = 0.;

        //INPUTS for expected outputs
        double swcExpected2[1] = {0.8259472};
        double hydredExpected2[1] = {5.821762e-05};

        //Begin TESTing when nlyrs = 1
        //Begin TEST for if swp[i] < swpwp[i] OR swp[j] < swpwp[j] AND lyrFrozen == false; j = i + 1
        hydraulic_redistribution(swc, swcwp, lyrRootCo, hydred, nlyrs,
          maxCondroot, swp50, shapeCond, scale);

        EXPECT_DOUBLE_EQ(hydred[0], 0); //no hydred in top layer

        EXPECT_NEAR(swc[0], swcExpected2[0], tol3); //swc is expected to match swcExpected
        EXPECT_NEAR(hydred[0], hydredExpected2[0], tol3); //hydred is expected to match hydredExpected

        //Reset to previous global states.
        Reset_SOILWAT2_after_UnitTest();
        // Setup soil layers
        create_test_soillayers(nlyrs);

        swc[0] = (s->lyr[0]->swcBulk_fieldcap + s->lyr[0]->swcBulk_wiltpt)
          / 2.;
        swcwp[0] = (s->lyr[0]->swcBulk_wiltpt) / 4;
        lyrRootCo[0] =  s->lyr[0]->transp_coeff[SW_SHRUB];
        st->lyrFrozen[0] = swFALSE;
        hydred[0] = 0.;

        //Begin TEST for if else ^^
        //INPUTS for expected outputs.
        double swcExpected3[1] = {0.8259472};
        double hydredExpected3[1] = {5.821762e-05};

        hydraulic_redistribution(swc, swcwp, lyrRootCo, hydred, nlyrs,
          maxCondroot, swp50, shapeCond, scale);

        EXPECT_DOUBLE_EQ(hydred[0], 0); //no hydred in top layer

        EXPECT_NEAR(swc[0], swcExpected3[0], tol3); //swc is expected to match swcExpected
        EXPECT_NEAR(hydred[0], hydredExpected3[0], tol3); //hydred is expected to match hydredExpected

      }
      //Reset to previous global states.
      Reset_SOILWAT2_after_UnitTest();
    }
  }
}
