#include "gtest/gtest.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../generic.h"
#include "../filefuncs.h"
#include "../myMemory.h"
#include "../SW_Defines.h"
#include "../SW_Files.h"
#include "../SW_Model.h"
#include "../SW_Site.h"
#include "../SW_SoilWater.h"
#include "../SW_VegProd.h"
#include "../SW_Site.h"
#include "../SW_Flow_lib.h"
#include "sw_testhelpers.h"



namespace{
  // Test the 'SW_SoilWater' function 'SW_VWCBulkRes'
  TEST(SWSoilWaterTest, VWCBulkRes){
    //declare mock INPUTS
    RealD fractionGravel = .1;
    RealD clay = .7;
    RealD sand = .2;
    RealD porosity = 1;

    RealD res = SW_VWCBulkRes(fractionGravel, sand, clay, porosity);
    // when clay > .6, we expect res == SW_MISSING since this isn't within reasonable
    // range
    EXPECT_DOUBLE_EQ(res, SW_MISSING);

    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    clay = .5;
    sand = .04;

    res = SW_VWCBulkRes(fractionGravel, sand, clay, porosity);
    // when sand < .05, we expect res == SW_MISSING since this isn't within reasonable
    // range
    EXPECT_DOUBLE_EQ(res, SW_MISSING);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    sand = .4;
    porosity = .4;

    res = SW_VWCBulkRes(fractionGravel, sand, clay, porosity);
    // when sand == .4, clay == .5, porosity == .4 and fractionGravel ==.1,
    // we expect res == .088373829599999967
    EXPECT_DOUBLE_EQ(res, .088373829599999967);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    porosity = .1;

    res = SW_VWCBulkRes(fractionGravel, sand, clay, porosity);
    // when sand == .4, clay == .5, porosity == .1 and fractionGravel ==.1,
    // we expect res == 0
    EXPECT_DOUBLE_EQ(res, 0);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }


  // Test the 'SW_SoilWater' function 'SW_SWC_adjust_snow'
  TEST(SWSoilWaterTest, SWCadjustSnow){
    // setup mock variables
    SW_Site.TminAccu2 = 0;
    SW_Model.doy = 1;
    SW_Site.RmeltMax = 1;
    SW_Site.RmeltMin = 0;
    SW_Site.lambdasnow = .1;
    SW_Site.TmaxCrit = 1;

    RealD temp_min = 0, temp_max = 10, ppt = 1, rain = 1.5, snow = 1.5,
    snowmelt = 1.2;

    SW_SWC_adjust_snow(temp_min, temp_max, ppt, &rain, &snow, &snowmelt);
    // when average temperature >= SW_Site.TminAccu2, we expect rain == ppt
    EXPECT_EQ(rain, 1);
    // when average temperature >= SW_Site.TminAccu2, we expect snow == 0
    EXPECT_EQ(snow, 0);
    // when temp_snow <= SW_Site.TmaxCrit, we expect snowmelt == 0
    EXPECT_EQ(snowmelt, 0);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    SW_Site.TminAccu2 = 6;

    SW_SWC_adjust_snow(temp_min, temp_max, ppt, &rain, &snow, &snowmelt);
    // when average temperature < SW_Site.TminAccu2, we expect rain == 0
    EXPECT_EQ(rain, 0);
    // when average temperature < SW_Site.TminAccu2, we expect snow == ppt
    EXPECT_EQ(snow, 1);
    // when temp_snow > SW_Site.TmaxCrit, we expect snowmelt == fmax(0, *snowpack - *snowmelt )
    EXPECT_EQ(snowmelt, 0);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    temp_max = 22;

    SW_SWC_adjust_snow(temp_min, temp_max, ppt, &rain, &snow, &snowmelt);
    // when average temperature >= SW_Site.TminAccu2, we expect rain == ppt
    EXPECT_EQ(rain, 1);
    // when average temperature >= SW_Site.TminAccu2, we expect snow == 0
    EXPECT_EQ(snow, 0);
    // when temp_snow > SW_Site.TmaxCrit, we expect snowmelt == 0
    EXPECT_EQ(snowmelt, 0);
  }


  // Test the 'SW_SoilWater' functions 'SWRC_SWCtoSWP' and `SWRC_SWPtoSWC`
  TEST(SWSoilWaterTest, TranslateBetweenSWCandSWP) {
    // set up mock variables
    RealD
      phi,
      swcBulk, swp, swc_fc, swc_wp,
      swrcp[SWRC_PARAM_NMAX],
      sand = 0.33,
      clay = 0.33,
      gravel = 0.2,
      width = 10.;
    unsigned int swrc_type, pdf_type;
    const int em = LOGFATAL;

    // Loop over SWRCs
    for (swrc_type = 0; swrc_type < N_SWRCs; swrc_type++) {
      memset(swrcp, 0., SWRC_PARAM_NMAX * sizeof(swrcp[0]));

      // Find a suitable PDF to generate `SWRCp`
      // (start `k2` at 1 because 0 codes to "NoPDF")
      for (
        pdf_type = 1;
        pdf_type < N_PDFs && !check_SWRC_vs_PDF(
          (char *) swrc2str[swrc_type],
          (char *) pdf2str[pdf_type],
          swTRUE
        );
        pdf_type++
      ) {}

      // Obtain SWRCp
      if (pdf_type < N_PDFs) {
        // PDF implemented in C: estimate parameters
        SWRC_PDF_estimate_parameters(
          pdf_type,
          swrcp,
          sand,
          clay,
          gravel
        );

      } else {
        // PDF not implemented in C: provide hard coded values
        if (
          Str_CompareI(
            (char *) swrc2str[swrc_type],
            (char *) "vanGenuchten1980"
          ) == 0
        ) {
          swrcp[0] = 0.11214750;
          swrcp[1] = 0.4213539;
          swrcp[2] = 0.007735474;
          swrcp[3] = 1.344678;

        } else {
          FAIL() << "No SWRC parameters available for " << swrc2str[swrc_type];
        }
      }


      //------ Tests SWC -> SWP

      // if swc > field capacity, then we expect phi < 0.33 bar
      swp = 1. / 3.;
      swc_fc = SWRC_SWPtoSWC(swp, swrc_type, swrcp, gravel, width, em);
      phi = SWRC_SWCtoSWP(swc_fc + 0.1, swrc_type, swrcp, gravel, width, em);
      EXPECT_LT(phi, swp);

      // if swc = field capacity, then we expect phi == 0.33 bar
      phi = SWRC_SWCtoSWP(swc_fc, swrc_type, swrcp, gravel, width, em);
      EXPECT_NEAR(phi, 1. / 3., tol9);

      // if field capacity > swc > wilting point, then
      // we expect 15 bar > phi > 0.33 bar
      swc_wp = SWRC_SWPtoSWC(15., swrc_type, swrcp, gravel, width, em);
      swcBulk = (swc_wp + swc_fc) / 2.;
      phi = SWRC_SWCtoSWP(swcBulk, swrc_type, swrcp, gravel, width, em);
      EXPECT_GT(phi, 1. / 3.);
      EXPECT_LT(phi, 15.);

      // if swc = wilting point, then we expect phi == 15 bar
      phi = SWRC_SWCtoSWP(swc_wp, swrc_type, swrcp, gravel, width, em);
      EXPECT_NEAR(phi, 15., tol9);

      // if swc < wilting point, then we expect phi > 15 bar
      swcBulk = SWRC_SWPtoSWC(2. * 15., swrc_type, swrcp, gravel, width, em);
      phi = SWRC_SWCtoSWP(swcBulk, swrc_type, swrcp, gravel, width, em);
      EXPECT_GT(phi, 15.);


      //------ Tests SWP -> SWC
      // when fractionGravel is 1, we expect theta == 0
      EXPECT_EQ(
        SWRC_SWPtoSWC(15., swrc_type, swrcp, 1., width, em),
        0.
      );

      // when width is 0, we expect theta == 0
      EXPECT_EQ(
        SWRC_SWPtoSWC(15., swrc_type, swrcp, gravel, 0., em),
        0.
      );

      // check bounds of swc
      swcBulk = SWRC_SWPtoSWC(15., swrc_type, swrcp, gravel, width, em);
      EXPECT_GE(swcBulk, 0.);
      EXPECT_LE(swcBulk, width * (1. - gravel));
    }
  }


  // Death Tests of 'SW_SoilWater' function 'SWRC_SWCtoSWP'
  TEST(SoilWaterDeathTest, SWCtoSWP) {
    // set up mock variables
    RealD
      swrcp[SWRC_PARAM_NMAX],
      gravel = 0.1,
      width = 10.;

    unsigned int swrc_type;


    //--- we expect (non-)fatal errors in a few situations
    // (fatality depends on the error mode)

    //--- 1) Unimplemented SWRC
    swrc_type = N_SWRCs + 1;
    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_SWCtoSWP(1., swrc_type, swrcp, gravel, width, LOGFATAL),
      "@ generic.c LogError"
    );
    EXPECT_DOUBLE_EQ(
      SWRC_SWCtoSWP(1., swrc_type, swrcp, gravel, width, LOGWARN),
      SW_MISSING
    );


    // --- 2) swc < 0: water content cannot be missing, zero or negative
    swrc_type = 0; // any SWRC
    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_SWCtoSWP(SW_MISSING, swrc_type, swrcp, gravel, width, LOGFATAL),
      "@ generic.c LogError"
    );
    EXPECT_DOUBLE_EQ(
      SWRC_SWCtoSWP(SW_MISSING, swrc_type, swrcp, gravel, width, LOGWARN),
      SW_MISSING
    );

    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_SWCtoSWP(-1., swrc_type, swrcp, gravel, width, LOGFATAL),
      "@ generic.c LogError"
    );
    EXPECT_DOUBLE_EQ(
      SWRC_SWCtoSWP(-1., swrc_type, swrcp, gravel, width, LOGWARN),
      SW_MISSING
    );

    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_SWCtoSWP(0., swrc_type, swrcp, gravel, width, LOGFATAL),
      "@ generic.c LogError"
    );
    EXPECT_DOUBLE_EQ(
      SWRC_SWCtoSWP(0., swrc_type, swrcp, gravel, width, LOGWARN),
      SW_MISSING
    );

    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_SWCtoSWP(1., swrc_type, swrcp, 1., width, LOGFATAL),
      "@ generic.c LogError"
    );
    EXPECT_DOUBLE_EQ(
      SWRC_SWCtoSWP(1., swrc_type, swrcp, 1., width, LOGWARN),
      SW_MISSING
    );

    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_SWCtoSWP(1., swrc_type, swrcp, gravel, 0., LOGFATAL),
      "@ generic.c LogError"
    );
    EXPECT_DOUBLE_EQ(
      SWRC_SWCtoSWP(1., swrc_type, swrcp, gravel, 0., LOGWARN),
      SW_MISSING
    );


    // --- 3) if theta_sat == 0 (specific to Campbell1974)
    // note: this case is normally prevented due to checks of inputs
    swrc_type = encode_str2swrc((char *) "Campbell1974");
    memset(swrcp, 0., SWRC_PARAM_NMAX * sizeof(swrcp[0]));
    swrcp[0] = 24.2159;
    swrcp[2] = 10.3860;

    swrcp[1] = 0.; // instead of 0.4436
    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_SWCtoSWP(5., swrc_type, swrcp, gravel, width, LOGFATAL),
      "@ generic.c LogError"
    );
    EXPECT_DOUBLE_EQ(
      SWRC_SWCtoSWP(5., swrc_type, swrcp, gravel, width, LOGWARN),
      SW_MISSING
    );


    // --- 4) if (theta - theta_res) <= 0 (specific to vanGenuchten1980)
    // note: this case is normally prevented due to SWC checks
    swrc_type = encode_str2swrc((char *) "vanGenuchten1980");
    memset(swrcp, 0., SWRC_PARAM_NMAX * sizeof(swrcp[0]));
    swrcp[0] = 0.1246;
    swrcp[1] = 0.4445;
    swrcp[2] = 0.0112;
    swrcp[3] = 1.2673;

    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_SWCtoSWP(0.99 * swrcp[0], swrc_type, swrcp, gravel, width, LOGFATAL),
      "@ generic.c LogError"
    );
    EXPECT_DOUBLE_EQ(
      SWRC_SWCtoSWP(0.99 * swrcp[0], swrc_type, swrcp, gravel, width, LOGWARN),
      SW_MISSING
    );
  }


  // Death Tests of 'SW_SoilWater' function 'SWRC_SWPtoSWC'
  TEST(SoilWaterDeathTest, SWPtoSWC) {
    // set up mock variables
    RealD
      swrcp[SWRC_PARAM_NMAX],
      gravel = 0.1,
      width = 10.;

    unsigned int swrc_type;


    //--- we expect (non-)fatal errors in two situations
    // (fatality depends on the error mode)

    //--- 1) Unimplemented SWRC
    swrc_type = N_SWRCs + 1;
    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_SWPtoSWC(0., swrc_type, swrcp, gravel, width, LOGFATAL),
      "@ generic.c LogError"
    );
    EXPECT_DOUBLE_EQ(
      SWRC_SWPtoSWC(0., swrc_type, swrcp, gravel, width, LOGWARN),
      SW_MISSING
    );

    // --- 2) swp <= 0: water content cannot be zero or negative (any SWRC)
    swrc_type = 0;
    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_SWPtoSWC(-1., swrc_type, swrcp, gravel, width, LOGFATAL),
      "@ generic.c LogError"
    );
    EXPECT_DOUBLE_EQ(
      SWRC_SWPtoSWC(-1., swrc_type, swrcp, gravel, width, LOGWARN),
      SW_MISSING
    );

    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_SWPtoSWC(0., swrc_type, swrcp, gravel, width, LOGFATAL),
      "@ generic.c LogError"
    );
    EXPECT_DOUBLE_EQ(
      SWRC_SWPtoSWC(0., swrc_type, swrcp, gravel, width, LOGWARN),
      SW_MISSING
    );
  }
}
