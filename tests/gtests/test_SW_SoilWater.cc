#include "gtest/gtest.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <iostream>
#include <math.h>
#include "include/generic.h"
#include "include/filefuncs.h"
#include "include/myMemory.h"
#include "include/SW_Defines.h"
#include "include/SW_Files.h"
#include "include/SW_Model.h"
#include "include/SW_Site.h"
#include "include/SW_SoilWater.h"
#include "include/SW_VegProd.h"
#include "include/SW_Flow_lib.h"
#include "tests/gtests/sw_testhelpers.h"

namespace{
  // Test the 'SW_SoilWater' function 'SW_SWC_adjust_snow'
  TEST(SWSoilWaterTest, SWCadjustSnow){
    // setup mock variables
    SW_All.Site.TminAccu2 = 0;
    SW_All.Model.doy = 1;
    SW_All.Site.RmeltMax = 1;
    SW_All.Site.RmeltMin = 0;
    SW_All.Site.lambdasnow = .1;
    SW_All.Site.TmaxCrit = 1;

    RealD temp_min = 0, temp_max = 10, ppt = 1, rain = 1.5, snow = 1.5,
    snowmelt = 1.2;

    SW_SWC_adjust_snow(&SW_All.Weather.temp_snow, SW_All.SoilWat.snowpack,
                       &SW_All.Site, temp_min, temp_max, ppt,
                       SW_All.Model.doy, &rain, &snow, &snowmelt);
    // when average temperature >= SW_Site.TminAccu2, we expect rain == ppt
    EXPECT_EQ(rain, 1);
    // when average temperature >= SW_All.Site.TminAccu2, we expect snow == 0
    EXPECT_EQ(snow, 0);
    // when temp_snow <= SW_All.Site.TmaxCrit, we expect snowmelt == 0
    EXPECT_EQ(snowmelt, 0);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    SW_All.Site.TminAccu2 = 6;

    SW_SWC_adjust_snow(&SW_All.Weather.temp_snow, SW_All.SoilWat.snowpack,
                       &SW_All.Site, temp_min, temp_max, ppt,
                       SW_All.Model.doy, &rain, &snow, &snowmelt);
    // when average temperature < SW_Site.TminAccu2, we expect rain == 0
    EXPECT_EQ(rain, 0);
    // when average temperature < SW_All.Site.TminAccu2, we expect snow == ppt
    EXPECT_EQ(snow, 1);
    // when temp_snow > SW_All.Site.TmaxCrit, we expect snowmelt == fmax(0, *snowpack - *snowmelt )
    EXPECT_EQ(snowmelt, 0);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    temp_max = 22;

    SW_SWC_adjust_snow(&SW_All.Weather.temp_snow, SW_All.SoilWat.snowpack,
                       &SW_All.Site, temp_min, temp_max, ppt,
                       SW_All.Model.doy, &rain, &snow, &snowmelt);
    // when average temperature >= SW_Site.TminAccu2, we expect rain == ppt
    EXPECT_EQ(rain, 1);
    // when average temperature >= SW_All.Site.TminAccu2, we expect snow == 0
    EXPECT_EQ(snow, 0);
    // when temp_snow > SW_All.Site.TmaxCrit, we expect snowmelt == 0
    EXPECT_EQ(snowmelt, 0);
  }


  // Test the 'SW_SoilWater' functions 'SWRC_SWCtoSWP' and `SWRC_SWPtoSWC`
  TEST(SWSoilWaterTest, TranslateBetweenSWCandSWP) {
    // set up mock variables
    unsigned int swrc_type, ptf_type, k;
    const int em = LOGFATAL;
    RealD
      phi,
      swcBulk, swc_sat, swc_fc, swc_wp,
      swp,
      swrcp[SWRC_PARAM_NMAX],
      sand = 0.33,
      clay = 0.33,
      gravel = 0.2,
      bdensity = 1.4,
      width = 10.,
      // SWP values in [0, Inf[ but FXW maxes out at 6178.19079 bar
      swpsb[12] = {
        0., 0.001, 0.01, 0.026, 0.027, 0.33, 15., 30., 100., 300., 1000., 6178.
      },
      // SWP values in [fc, Inf[ but FXW maxes out at 6178.19079 bar
      swpsi[7] = {0.33, 15., 30., 100., 300., 1000., 6178.};

    std::ostringstream msg;



    // Loop over SWRCs
    for (swrc_type = 0; swrc_type < N_SWRCs; swrc_type++) {
      memset(swrcp, 0., SWRC_PARAM_NMAX * sizeof(swrcp[0]));

      // Find a suitable PTF to generate `SWRCp`
      for (
        ptf_type = 0;
        ptf_type < N_PTFs && !check_SWRC_vs_PTF(
          (char *) swrc2str[swrc_type],
          (char *) ptf2str[ptf_type]
        );
        ptf_type++
      ) {}


      // Obtain SWRCp
      if (ptf_type < N_PTFs) {
        // PTF implemented in C: estimate parameters
        SWRC_PTF_estimate_parameters(
          ptf_type,
          swrcp,
          sand,
          clay,
          gravel,
          bdensity
        );

      } else {
        // PTF not implemented in C: provide hard coded values
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
          swrcp[4] = 7.78506;

        } else if (
          Str_CompareI(
            (char *) swrc2str[swrc_type],
            (char *) "FXW"
          ) == 0
        ) {
          swrcp[0] = 0.437461;
          swrcp[1] = 0.050757;
          swrcp[2] = 1.247689;
          swrcp[3] = 0.308681;
          swrcp[4] = 22.985379;
          swrcp[5] = 2.697338;

        } else {
          FAIL() << "No SWRC parameters available for " << swrc2str[swrc_type];
        }
      }


      //------ Tests SWC -> SWP
      msg.str("");
      msg << "SWRC/PTF = " << swrc_type << "/" << ptf_type;

      // preferably we would print names instead of type codes, but
      // this leads to "global-buffer-overflow"
      // 0 bytes to the right of global variable 'ptf2str'
      //msg << "SWRC/PTF = " << swrc2str[swrc_type] << "/" << ptf2str[ptf_type];

      swc_sat = SWRC_SWPtoSWC(0., swrc_type, swrcp, gravel, width, em);
      swc_fc = SWRC_SWPtoSWC(1. / 3., swrc_type, swrcp, gravel, width, em);
      swc_wp = SWRC_SWPtoSWC(15., swrc_type, swrcp, gravel, width, em);


      // if swc = saturation, then we expect phi in [0, fc]
      // for instance, Campbell1974 goes to (theta_sat, swrcp[0]) instead of 0
      swp = SWRC_SWCtoSWP(swc_sat, swrc_type, swrcp, gravel, width, em);
      EXPECT_GE(swp, 0.) << msg.str();
      EXPECT_LT(swp, 1. / 3.) << msg.str();


      // if swc > field capacity, then we expect phi < 0.33 bar
      swcBulk = (swc_sat + swc_fc) / 2.;
      EXPECT_LT(
        SWRC_SWCtoSWP(swcBulk, swrc_type, swrcp, gravel, width, em),
        1. / 3.
      ) << msg.str();

      // if swc = field capacity, then we expect phi == 0.33 bar
      EXPECT_NEAR(
        SWRC_SWCtoSWP(swc_fc, swrc_type, swrcp, gravel, width, em),
        1. / 3.,
        tol9
      ) << msg.str();

      // if field capacity > swc > wilting point, then
      // we expect 15 bar > phi > 0.33 bar
      swcBulk = (swc_wp + swc_fc) / 2.;
      phi = SWRC_SWCtoSWP(swcBulk, swrc_type, swrcp, gravel, width, em);
      EXPECT_GT(phi, 1. / 3.) << msg.str();
      EXPECT_LT(phi, 15.) << msg.str();

      // if swc = wilting point, then we expect phi == 15 bar
      EXPECT_NEAR(
        SWRC_SWCtoSWP(swc_wp, swrc_type, swrcp, gravel, width, em),
        15.,
        tol9
      ) << msg.str();

      // if swc < wilting point, then we expect phi > 15 bar
      swcBulk = SWRC_SWPtoSWC(2. * 15., swrc_type, swrcp, gravel, width, em);
      EXPECT_GT(
        SWRC_SWCtoSWP(swcBulk, swrc_type, swrcp, gravel, width, em),
        15.
      ) << msg.str();



      //------ Tests SWP -> SWC
      // when fractionGravel is 1, we expect theta == 0
      EXPECT_EQ(
        SWRC_SWPtoSWC(15., swrc_type, swrcp, 1., width, em),
        0.
      ) << msg.str();

      // when width is 0, we expect theta == 0
      EXPECT_EQ(
        SWRC_SWPtoSWC(15., swrc_type, swrcp, gravel, 0., em),
        0.
      ) << msg.str();

      // check bounds of swc
      for (k = 0; k < length(swpsb); k++) {
        swcBulk = SWRC_SWPtoSWC(swpsb[k], swrc_type, swrcp, gravel, width, em);
        EXPECT_GE(swcBulk, 0.) <<
          msg.str() << " at SWP = " << swpsb[k] << " bar";
        EXPECT_LE(swcBulk, width * (1. - gravel)) <<
          msg.str() << " at SWP = " << swpsb[k] << " bar";
      }


      //------ Tests that both SWP <-> SWC are inverse of each other
      // for phi at 0 (saturation) and phi in [fc, infinity]
      // but not necessarily if phi in ]0, fc[;
      // for instance, Campbell1974 is not inverse in ]0, swrcp[0][
      for (k = 0; k < length(swpsi); k++) {
        swcBulk = SWRC_SWPtoSWC(swpsi[k], swrc_type, swrcp, gravel, width, em);
        swp = SWRC_SWCtoSWP(swcBulk, swrc_type, swrcp, gravel, width, em);

        EXPECT_NEAR(swp, swpsi[k], tol9) <<
          msg.str() << " at SWP = " << swpsi[k] << " bar";
        EXPECT_NEAR(
          SWRC_SWPtoSWC(swp, swrc_type, swrcp, gravel, width, em),
          swcBulk,
          tol9
        ) << msg.str() << " at SWC = " << swcBulk << " cm";
      }
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
      "is not implemented"
    );
    EXPECT_DOUBLE_EQ(
      SWRC_SWCtoSWP(1., swrc_type, swrcp, gravel, width, LOGWARN),
      SW_MISSING
    );


    // --- 2) swc < 0: water content cannot be negative
    for (swrc_type = 0; swrc_type < N_SWRCs; swrc_type++) {
      EXPECT_DEATH_IF_SUPPORTED(
        SWRC_SWCtoSWP(-1., swrc_type, swrcp, gravel, width, LOGFATAL),
        "invalid SWC"
      );
      EXPECT_DOUBLE_EQ(
        SWRC_SWCtoSWP(-1., swrc_type, swrcp, gravel, width, LOGWARN),
        SW_MISSING
      );

      EXPECT_DEATH_IF_SUPPORTED(
        SWRC_SWCtoSWP(1., swrc_type, swrcp, 1., width, LOGFATAL),
        "invalid SWC"
      );
      EXPECT_DOUBLE_EQ(
        SWRC_SWCtoSWP(1., swrc_type, swrcp, 1., width, LOGWARN),
        SW_MISSING
      );

      EXPECT_DEATH_IF_SUPPORTED(
        SWRC_SWCtoSWP(1., swrc_type, swrcp, gravel, 0., LOGFATAL),
        "invalid SWC"
      );
      EXPECT_DOUBLE_EQ(
        SWRC_SWCtoSWP(1., swrc_type, swrcp, gravel, 0., LOGWARN),
        SW_MISSING
      );
    }

    // --- *) if (theta - theta_res) < 0 (specific to vanGenuchten1980)
    // note: this case is normally prevented due to SWC checks
    swrc_type = encode_str2swrc((char *) "vanGenuchten1980");
    memset(swrcp, 0., SWRC_PARAM_NMAX * sizeof(swrcp[0]));
    swrcp[0] = 0.1246;
    swrcp[1] = 0.4445;
    swrcp[2] = 0.0112;
    swrcp[3] = 1.2673;
    swrcp[4] = 7.78506;

    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_SWCtoSWP(0.99 * swrcp[0], swrc_type, swrcp, gravel, width, LOGFATAL),
      "invalid value"
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
      SWRC_SWPtoSWC(15., swrc_type, swrcp, gravel, width, LOGFATAL),
      "is not implemented"
    );
    EXPECT_DOUBLE_EQ(
      SWRC_SWPtoSWC(15., swrc_type, swrcp, gravel, width, LOGWARN),
      SW_MISSING
    );

    // --- 2) swp < 0: water content cannot be negative (any SWRC)
    for (swrc_type = 0; swrc_type < N_SWRCs; swrc_type++) {
      EXPECT_DEATH_IF_SUPPORTED(
        SWRC_SWPtoSWC(-1., swrc_type, swrcp, gravel, width, LOGFATAL),
        "invalid SWP"
      );
      EXPECT_DOUBLE_EQ(
        SWRC_SWPtoSWC(-1., swrc_type, swrcp, gravel, width, LOGWARN),
        SW_MISSING
      );
    }
  }
}
