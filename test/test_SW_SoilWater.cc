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


  // Test the 'SW_SoilWater' function 'SWRC_SWCtoSWP'
  TEST(SWSoilWaterTest, SWCtoSWP) {
    // set up mock variables
    RealD
      res,
      swcBulk, swp, swc_fc, swc_wp,
      swrcp[SWRC_PARAM_NMAX],
      sand = 0.33,
      clay = 0.33,
      gravel = 0.2,
      width = 10.;

    //--- Cosby et al. 1984 PDF for Campbell's 1974 SWRC
    unsigned int
      swrc_type = encode_str2swrc(Str_Dup("Campbell1974")),
      pdf_type = encode_str2pdf(Str_Dup("Cosby1984AndOthers"));

    SWRC_PDF_estimate_parameters(
      pdf_type,
      swrcp,
      sand,
      clay,
      gravel
    );

    // when swc is 0, we expect res == 0
    res = SWRC_SWCtoSWP(0., swrc_type, swrcp, gravel, width);
    EXPECT_EQ(res, 0.0);

    // when swc is SW_MISSING, we expect res == 0
    res = SWRC_SWCtoSWP(SW_MISSING, swrc_type, swrcp, gravel, width);
    EXPECT_EQ(res, 0.0);

    // if swc > field capacity, then we expect res < 0.33 bar
    swp = 1. / 3.;
    swc_fc = SWRC_SWPtoSWC(swp, swrc_type, swrcp, gravel, width);
    res = SWRC_SWCtoSWP(swc_fc + 0.1, swrc_type, swrcp, gravel, width);
    EXPECT_LT(res, swp);

    // if swc = field capacity, then we expect res == 0.33 bar
    res = SWRC_SWCtoSWP(swc_fc, swrc_type, swrcp, gravel, width);
    EXPECT_NEAR(res, 1. / 3., tol9);

    // if field capacity > swc > wilting point, then
    // we expect 15 bar > res > 0.33 bar
    swc_wp = SWRC_SWPtoSWC(15., swrc_type, swrcp, gravel, width);
    swcBulk = (swc_wp + swc_fc) / 2.;
    res = SWRC_SWCtoSWP(swcBulk, swrc_type, swrcp, gravel, width);
    EXPECT_GT(res, 1. / 3.);
    EXPECT_LT(res, 15.);

    // if swc = wilting point, then we expect res == 15 bar
    res = SWRC_SWCtoSWP(swc_wp, swrc_type, swrcp, gravel, width);
    EXPECT_NEAR(res, 15., tol9);

    // if swc < wilting point, then we expect res > 15 bar
    res = SWRC_SWCtoSWP(swc_wp / 2., swrc_type, swrcp, gravel, width);
    EXPECT_GT(res, 15.);

    // --- 3a) if theta1 == 0 (e.g., gravel == 1)
    res = SWRC_SWCtoSWP(swc_wp, swrc_type, swrcp, 1., width);
    EXPECT_DOUBLE_EQ(res, 0.);
  }


  TEST(SoilWaterDeathTest, SWCtoSWP) {
    // set up mock variables
    RealD
      swrcp[SWRC_PARAM_NMAX],
      sand = 0.33,
      clay = 0.33,
      gravel = 0.1,
      width = 10.;

    unsigned int swrc_type, pdf_type;


    //--- we expect fatal errors in three situations

    //--- 1) Unimplemented SWRC
    swrc_type = 255;
    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_SWCtoSWP(1., swrc_type, swrcp, gravel, width),
      "@ generic.c LogError"
    );


    //--- Cosby et al. 1984 PDF for Campbell's 1974 SWRC
    swrc_type = encode_str2swrc(Str_Dup("Campbell1974"));
    pdf_type = encode_str2pdf(Str_Dup("Cosby1984AndOthers"));

    SWRC_PDF_estimate_parameters(
      pdf_type,
      swrcp,
      sand,
      clay,
      gravel
    );

    // --- 2) swc < 0: water content cannot be negative
    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_SWCtoSWP(-1., swrc_type, swrcp, gravel, width),
      "@ generic.c LogError"
    );


    // --- 3) if theta_sat == 0
    // note: this case is normally prevented due to checks of inputs by
    // function `SWRC_check_parameters_for_Campbell1974` and
    // function `_read_layers`
    swrcp[1] = 0.;
    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_SWCtoSWP(5., swrc_type, swrcp, gravel, width),
      "@ generic.c LogError"
    );
  }


  // Test the 'SW_SoilWater' function 'SWRC_SWPtoSWC'
  TEST(SWSoilWaterTest, SWPtoSWC) {
    // set up mock variables
    RealD
      res,
      swpMatric = 15.0,
      swrcp[SWRC_PARAM_NMAX],
      sand = 0.33,
      clay = 0.33,
      gravel,
      width = 10.;
    short i;

    //--- Campbell's 1974 SWRC (using Cosby et al. 1984 PDF)
    unsigned int
      swrc_type = encode_str2swrc(Str_Dup("Campbell1974")),
      pdf_type = encode_str2pdf(Str_Dup("Cosby1984AndOthers"));

    // set gravel fractions on the interval [.0, 1], step .1
    for (i = 0; i <= 10; i++) {
      gravel = i / 10.;

      SWRC_PDF_estimate_parameters(
        pdf_type,
        swrcp,
        sand,
        clay,
        gravel
      );

      res = SWRC_SWPtoSWC(swpMatric, swrc_type, swrcp, gravel, width);

      EXPECT_GE(res, 0.);
      EXPECT_LE(res, width * (1. - gravel));
    }


    // when fractionGravel is 1, we expect t == 0
    EXPECT_EQ(
      SWRC_SWPtoSWC(swpMatric, swrc_type, swrcp, 1., width),
      0.
    );

    // when width is 0, we expect t == 0
    EXPECT_EQ(
      SWRC_SWPtoSWC(swpMatric, swrc_type, swrcp, 0., 0.),
      0.
    );
  }


  TEST(SoilWaterDeathTest, SWPtoSWC) {
    // set up mock variables
    RealD
      swrcp[SWRC_PARAM_NMAX],
      sand = 0.33,
      clay = 0.33,
      gravel = 0.1,
      width = 10.;

    unsigned int swrc_type, pdf_type;


    //--- we expect fatal errors in two situations

    //--- 1) Unimplemented SWRC
    swrc_type = 255;
    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_SWPtoSWC(0., swrc_type, swrcp, gravel, width),
      "@ generic.c LogError"
    );


    //--- Cosby et al. 1984 PDF for Campbell's 1974 SWRC
    swrc_type = encode_str2swrc(Str_Dup("Campbell1974"));
    pdf_type = encode_str2pdf(Str_Dup("Cosby1984AndOthers"));

    SWRC_PDF_estimate_parameters(
      pdf_type,
      swrcp,
      sand,
      clay,
      gravel
    );

    // --- 2) swp <= 0: water content cannot be negative
    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_SWPtoSWC(-1., swrc_type, swrcp, gravel, width),
      "@ generic.c LogError"
    );
  }
}
