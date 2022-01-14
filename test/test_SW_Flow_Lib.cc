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


  //Test transp_weighted_avg function.
  TEST(SWFlowTest, transp_weighted_avg)
  {
    //--- TEST when n_layers is 1 ------
    //INPUTS
    double swp_avg = 10;
    unsigned int i;
    unsigned int n_tr_rgns = 1, n_layers = 1;
    unsigned int tr_regions[1] = {1}; // 1-4
    double tr_coeff[1] = {0.0496}; //trco_grass
    double swc[1] = {12};

    //INPUTS for expected outputs
    double swp_avgExpected1 = 1.5992088;

    // Setup soil layers
    create_test_soillayers(n_layers);

    ForEachSoilLayer(i)
    {
      // copy soil layer values into arrays so that they can be passed as
      // arguments to `transp_weighted_avg`
      tr_coeff[i] = s->lyr[i]->transp_coeff[SW_SHRUB];

      // example: swc as mean of wilting point and field capacity
      swc[i] = (s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt) / 2.;
    }

    //Begin TEST when n_layers is one
    transp_weighted_avg(&swp_avg, n_tr_rgns, n_layers, tr_regions, tr_coeff,
                        swc);

    EXPECT_GE(swp_avg, 0); //Must always be non negative.
    EXPECT_NEAR(swp_avg, swp_avgExpected1, tol6);

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();

    //--- TEST when n_layers is at "max" ------
    //INPUTS
    swp_avg = 10, n_tr_rgns = 4, n_layers = 25;
    unsigned int tr_regions2[25] = {1,1,1,2,2,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,
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


    EXPECT_NEAR(swp_avg, swp_avgExpectedM, tol6);

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
    double swc_init[MAX_LAYERS], swc[MAX_LAYERS];
    double swcmin[MAX_LAYERS] = {0.};
    double qty[MAX_LAYERS] = {0.}, qty_sum = 0.;
    double coeff[MAX_LAYERS], coeffZero[MAX_LAYERS] = {0.};


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

      // Setup: soil layers
      create_test_soillayers(nlyrs);

      ForEachSoilLayer(i)
      {
        // Setup: initial swc to some example value, here SWC at 20% VWC
        swc_init[i] = 0.2 * s->lyr[i]->width;
        // Setup: water extraction coefficient, some example value, here 0.5
        coeff[i] = 0.5;
      }

      //------ 1) TEST: if coeff[i] == 0, then expectation: no water extracted
      // Re-set inputs
      aet = aet_init;
      ForEachSoilLayer(i)
      {
        qty[i] = 0.;
        swc[i] = swc_init[i];
      }

      // Call function to test: use coeffZero instead of coeff
      remove_from_soil(swc, qty, &aet, nlyrs, coeffZero, rate, swcmin);

      // Check expectation of no change from original values
      qty_sum = 0.;
      for (i = 0; i < nlyrs; i++)
      {
        EXPECT_NEAR(qty[i], 0., tol6) <<
          "remove_from_soil: qty != qty for layer " << 1 + i << " out of "
          << nlyrs << " soil layers";
        EXPECT_NEAR(swc[i], swc_init[i], tol6) <<
          "remove_from_soil: swc != swc for layer " << 1 + i << " out of "
          << nlyrs << " soil layers";
        qty_sum += qty[i];
      }
      EXPECT_DOUBLE_EQ(aet, aet_init) <<
          "remove_from_soil(no coeff): aet != aet for " << nlyrs <<
          " soil layers";
      EXPECT_DOUBLE_EQ(qty_sum, 0.) <<
          "remove_from_soil: sum(qty) != 0 for " << nlyrs << " soil layers";


      //------ 2) TEST: if frozen[i], then expectation: no water extracted
      // Re-set inputs and set soil layers as frozen
      aet = aet_init;
      ForEachSoilLayer(i)
      {
        st->lyrFrozen[i] = swTRUE;
        qty[i] = 0.;
        swc[i] = swc_init[i];
      }

      // Call function to test
      remove_from_soil(swc, qty, &aet, nlyrs, coeff, rate, swcmin);

      // Check expectation of no change from original values
      qty_sum = 0.;
      for (i = 0; i < nlyrs; i++)
      {
        EXPECT_NEAR(qty[i], 0., tol6) <<
          "remove_from_soil(frozen): qty != qty for layer " << 1 + i <<
          " out of " << nlyrs << " soil layers";
        EXPECT_NEAR(swc[i], swc_init[i], tol6) <<
          "remove_from_soil(frozen): swc != swc for layer " << 1 + i <<
          " out of " << nlyrs << " soil layers";
        qty_sum += qty[i];
      }
      EXPECT_DOUBLE_EQ(aet, aet_init) <<
          "remove_from_soil(frozen): aet != aet for " << nlyrs <<
          " soil layers";
      EXPECT_DOUBLE_EQ(qty_sum, 0.) <<
          "remove_from_soil: sum(qty) != 0 for " << nlyrs << " soil layers";


      //------ 3) TEST: if coeff[i] > 0 && !frozen[i], then water is extracted
      // Re-set inputs
      aet = aet_init;
      ForEachSoilLayer(i)
      {
        st->lyrFrozen[i] = swFALSE;
        qty[i] = 0.;
        swc[i] = swc_init[i];
      }

      // Call function to test
      remove_from_soil(swc, qty, &aet, nlyrs, coeff, rate, swcmin);

      // Check values of qty[] and swc[]
      qty_sum = 0.;
      for (i = 0; i < nlyrs; i++)
      {
        // Check that swc_init > qty (removed amount of water) > 0
        EXPECT_GT(qty[i], 0.) <<
          "remove_from_soil: qty !> 0 in layer "
          << 1 + i << " out of " << nlyrs << " soil layers";
        EXPECT_LT(qty[i], swc_init[i]) <<
          "remove_from_soil: qty !< swc_init in layer "
          << 1 + i << " out of " << nlyrs << " soil layers";

        // Check that swc_init > swc > swc_min
        EXPECT_GT(swc[i], swcmin[i]) <<
          "remove_from_soil: qty !> swc_min in layer "
          << 1 + i << " out of " << nlyrs << " soil layers";
        EXPECT_LT(swc[i], swc_init[i]) <<
          "remove_from_soil: qty !< swc_init in layer "
          << 1 + i << " out of " << nlyrs << " soil layers";

        // Check that swc_init = swc + qty
        EXPECT_NEAR(swc[i] + qty[i], swc_init[i], tol6) <<
          "remove_from_soil: swc + qty != swc_init in layer "
          << 1 + i << " out of " << nlyrs << " soil layers";

        qty_sum += qty[i];
      }

      // Check that aet - aet_init = sum(qty)
      EXPECT_NEAR(aet, aet_init + qty_sum, tol6) <<
          "remove_from_soil: delta(aet) != sum(qty) for "
          << nlyrs << " soil layers";

      // Check that rate >= sum(qty) > 0
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
  TEST(SWFlowTest, PercolateUnsaturated)
  {
    //INPUTS
    unsigned int nlyrs, i, k;
    double
      sum_delta_swc, small, drainout, standingWater;
    double swc[MAX_LAYERS];
    double drain[MAX_LAYERS];


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

      // Initialize soil arrays to be independent of soil texture...
      ForEachSoilLayer(i)
      {
        SW_Site.lyr[i]->swcBulk_fieldcap = 0.25 * SW_Site.lyr[i]->width;
        SW_Site.lyr[i]->swcBulk_min = 0.05 * SW_Site.lyr[i]->width;
        SW_Site.lyr[i]->swcBulk_saturated = 0.35 * SW_Site.lyr[i]->width;
      }


      //--- (1) TEST:
      //        if swc[i] <= swcmin[i],
      //        then expect drain = 0

      // Set initial values
      drainout = 0.;
      standingWater = 0;

      ForEachSoilLayer(i)
      {
        SW_Site.lyr[i]->swcBulk_init = 0.5 * SW_Site.lyr[i]->swcBulk_min;
        swc[i] = SW_Site.lyr[i]->swcBulk_init;
        drain[i] = 0.;
      }

      // Call function to test
      percolate_unsaturated(
        swc, drain, &drainout, &standingWater,
        nlyrs, SW_Site.lyr, stValues.lyrFrozen,
        SW_Site.slow_drain_coeff, SLOW_DRAIN_DEPTH
      );

      // Expectation: drainout = 0
      EXPECT_NEAR(drainout, 0., tol6);

      // Expectation: standingWater = 0
      EXPECT_NEAR(standingWater, 0., tol6);

      // Expectations: (i) drain[i] = 0; (ii) delta(swc[i]) = 0
      ForEachSoilLayer(i)
      {
        EXPECT_NEAR(drain[i], 0., tol6) <<
          "percolate_unsaturated: drain != 0 for layer " << 1 + i;
        EXPECT_NEAR(swc[i], SW_Site.lyr[i]->swcBulk_init, tol6) <<
          "percolate_unsaturated: swc != swc_init for layer " << 1 + i;
      }


      //--- (2) TEST:
      //        if swc_fc > swc[i] > swcmin[i],
      //        then expect drain > 0

      // Set initial values
      drainout = 0.;
      standingWater = 0;

      ForEachSoilLayer(i)
      {
        SW_Site.lyr[i]->swcBulk_init = 0.9 * SW_Site.lyr[i]->swcBulk_fieldcap;
        swc[i] = SW_Site.lyr[i]->swcBulk_init;
        drain[i] = 0.;
      }

      // Call function to test
      percolate_unsaturated(
        swc, drain, &drainout, &standingWater,
        nlyrs, SW_Site.lyr, stValues.lyrFrozen,
        SW_Site.slow_drain_coeff, SLOW_DRAIN_DEPTH
      );

      // Expectation: drainout > 0
      EXPECT_GT(drainout, 0.);

      // Expectation: standingWater = 0
      EXPECT_NEAR(standingWater, 0., tol6);

      // Expectations: (i) drain[i] > 0; (ii) sum(delta(swc[i])) < 0
      sum_delta_swc = 0.;
      ForEachSoilLayer(i)
      {
        EXPECT_GT(drain[i], 0.) <<
          "percolate_unsaturated: drain !> 0 for layer " << 1 + i;
        sum_delta_swc += swc[i] - SW_Site.lyr[i]->swcBulk_init;
      }
      EXPECT_LT(sum_delta_swc, 0.) <<
        "percolate_unsaturated: sum(delta(swc[i])) !< 0 for layer " << 1 + i;


      //--- (3) TEST:
      //        if swc_sat ~ swc[i] > swc_fc[i],
      //        then expect drain < 0 && ponded > 0

      // Set initial values
      drainout = 0.;
      standingWater = 0;

      ForEachSoilLayer(i)
      {
        SW_Site.lyr[i]->swcBulk_init = 1.1 * SW_Site.lyr[i]->swcBulk_saturated;
        swc[i] = SW_Site.lyr[i]->swcBulk_init;
        drain[i] = 0.;
      }

      // Call function to test
      percolate_unsaturated(
        swc, drain, &drainout, &standingWater,
        nlyrs, SW_Site.lyr, stValues.lyrFrozen,
        SW_Site.slow_drain_coeff, SLOW_DRAIN_DEPTH
      );

      // Expectation: drainout > 0
      EXPECT_GT(drainout, 0.);

      // Expectation: standingWater > 0
      EXPECT_GT(standingWater, 0.);

      // Expectations: (i) drain[i] < 0; (ii) sum(delta(swc[i])) < 0
      sum_delta_swc = 0.;
      ForEachSoilLayer(i)
      {
        if (i + 1 < nlyrs) {
          EXPECT_LT(drain[i], 0.) <<
            "percolate_unsaturated: drain !< 0 for layer " << 1 + i;
        } else {
          EXPECT_NEAR(drain[i], SW_Site.slow_drain_coeff, tol6) <<
            "percolate_unsaturated: drain != sdrainpar in last layer " << 1 + i;
        }
        sum_delta_swc += swc[i] - SW_Site.lyr[i]->swcBulk_init;
      }
      EXPECT_LT(sum_delta_swc, 0.) <<
        "percolate_unsaturated: sum(delta(swc[i])) !< 0 for layer " << 1 + i;


      //--- (4) TEST:
      //        if lyrFrozen[i],
      //        then expect drain[i] to be small
      small = tol3;

      // Set initial values
      drainout = 0.;
      standingWater = 0;

      ForEachSoilLayer(i)
      {
        SW_Site.lyr[i]->swcBulk_init = 0.9 * SW_Site.lyr[i]->swcBulk_fieldcap;
        swc[i] = SW_Site.lyr[i]->swcBulk_init;
        drain[i] = 0.;
        stValues.lyrFrozen[i] = swTRUE;
      }

      // Call function to test
      percolate_unsaturated(
        swc, drain, &drainout, &standingWater,
        nlyrs, SW_Site.lyr, stValues.lyrFrozen,
        SW_Site.slow_drain_coeff, SLOW_DRAIN_DEPTH
      );


      // Expectation: small > drainout > 0
      EXPECT_GT(drainout, 0.);
      EXPECT_LT(drainout, small);

      // Expectation: standingWater = 0
      EXPECT_NEAR(standingWater, 0., tol6);

      // Expectations: (i) small > drain[i] > 0; (ii) delta(swc[i]) ~ 0
      ForEachSoilLayer(i)
      {
        EXPECT_GT(drain[i], 0.) <<
          "percolate_unsaturated: drain !> 0 for layer " << 1 + i;
        EXPECT_LT(drain[i], small) <<
          "percolate_unsaturated: small !> drain for layer " << 1 + i;
        EXPECT_NEAR(swc[i], SW_Site.lyr[i]->swcBulk_init, small) <<
          "percolate_unsaturated: swc !~ swc_init for layer " << 1 + i;
      }

      // Reset frozen status
      ForEachSoilLayer(i)
      {
        stValues.lyrFrozen[i] = swFALSE;
      }
    }

    // Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }


  //TEST for hydraulic_redistribution when nlyrs = MAX_LAYERS and nlyrs = 1
  TEST(SWFlowTest, hydraulic_redistribution)
  {
    // INPUTS
    unsigned int nlyrs, i, k;
    double maxCondroot = -0.2328, swp50 = 1.2e12, shapeCond = 1, scale = 0.3;
    double
      swc[MAX_LAYERS],
      hydred[MAX_LAYERS] = {0.};
    double swcExpected = 0., hydredExpected = 0.;

    // INPUTS for expected outcomes
    double swcExpected_1L[1] = {0.8258887};
    double hydredExpected_1L[1] = {0.};

    double swcExpected_MAXL[MAX_LAYERS] = {
      0.8258890, 0.2068467, 0.9920907, 0.2581966, 0.2329534,
      1.8503562, 0.1678064, 0.1678063, 0.4403078, 0.9193770,
      2.2045783, 0.2295204, 0.2329534, 1.8503562, 0.1678063,
      0.1678063, 0.1466935, 0.1838611, 0.2205380, 1.1471038,
      2.3287794, 2.3129346, 1.6781799, 3.3564146, 6.7275094};
    double hydredExpected_MAXL[MAX_LAYERS] = {
      0.000000e+00, -2.436254e-05, 3.723615e-05, 1.105724e-04, 7.844259e-05,
      3.179664e-05, -1.7262914e-05, -1.726291e-05, -1.6618943e-04, -5.0191450e-05,
      1.491759e-05, 1.105724e-04, 7.844259e-05, 3.1796639e-05, -1.72629141e-05,
      -1.726291e-05, -1.311540e-04, -2.436254e-05, 8.168036e-05, 5.476663e-05,
      2.937160e-05, 2.906830e-05, -5.625116e-05, -5.774957e-05, -1.093454e-04};

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
        // example data based on soil:
        swc[i] = (s->lyr[i]->swcBulk_fieldcap + s->lyr[i]->swcBulk_wiltpt) / 2.;
        stValues.lyrFrozen[i] = swFALSE;
      }

      // Call function to be tested
      hydraulic_redistribution(
        swc,
        hydred,
        SW_SHRUB,
        nlyrs,
        SW_Site.lyr,
        stValues.lyrFrozen,
        maxCondroot, swp50, shapeCond, scale
      );

      // Expection: no hydred in top layer
      EXPECT_DOUBLE_EQ(hydred[0], 0.);

      // Expections: depending on number of soil layers
      ForEachSoilLayer(i)
      {
        if (k == 0)
        {
          swcExpected = swcExpected_1L[i];
          hydredExpected = hydredExpected_1L[i];

        } else if (k == 1)
        {
          swcExpected = swcExpected_MAXL[i];
          hydredExpected = hydredExpected_MAXL[i];
        }

        EXPECT_NEAR(swc[i], swcExpected, tol6) <<
          "hydraulic_redistribution: swc != swcExpected0 for layer " <<
          1 + i << " out of " << nlyrs << " soil layers";

        EXPECT_NEAR(hydred[i], hydredExpected, tol6) <<
          "hydraulic_redistribution: hydred != hydredExpected0 for layer " <<
          1 + i << " out of "<< nlyrs << " soil layers";
      }

      // Reset to previous global states.
      Reset_SOILWAT2_after_UnitTest();
    }
  }
}
