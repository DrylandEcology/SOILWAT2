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
#include "../SW_Site.h" // externs `_TranspRgnBounds`
#include "../SW_VegProd.h"
#include "../SW_VegEstab.h"
#include "../SW_Model.h"
#include "../SW_SoilWater.h"
#include "../SW_Weather.h"
#include "../SW_Markov.h"
#include "../SW_Sky.h"

#include "sw_testhelpers.h"



namespace {
  // Test pedotransfer functions
  TEST(SWSiteTest, PDFs) {
    // inputs
    RealD
      swrcp[SWRC_PARAM_NMAX],
      swc_sat,
      sand = 0.33,
      clay = 0.33,
      gravel = 0.1,
      width = 10.;
    unsigned int swrc_type, pdf_type;


    //--- Test Cosby et al. 1984 PDF for Campbell's 1974 SWRC
    swrc_type = 1;
    pdf_type = 1;

    SWRC_PDF_estimate_parameters(
      swrc_type, pdf_type,
      swrcp,
      sand, clay, gravel
    );

    EXPECT_EQ(
      SWRC_check_parameters(swrc_type, swrcp),
      swTRUE
    );

    // Test psisMatric
    EXPECT_GT(swrcp[0], 3.890451); /* Value should always be greater
    than 3.890451 based upon complete consideration of potential range of sand and clay values */
    EXPECT_LT(swrcp[0],  34.67369); /* Value should always be less
    than 34.67369 based upon complete consideration of potential range of sand and clay values */
    EXPECT_DOUBLE_EQ(swrcp[0], 27.586715750763947); /* If sand is
    .33 and clay is .33, psisMatric should be 27.5867 */

    // Test thetasMatric
    EXPECT_GT(swrcp[1], 0.363); /* Value should always be greater
    than 36.3 based upon complete consideration of potential range of sand and clay values */
    EXPECT_LT(swrcp[1], 0.468); /* Value should always be less
    than 46.8 based upon complete consideration of potential range of sand and clay values */
    EXPECT_DOUBLE_EQ(swrcp[1], 0.44593); /* If sand is .33 and
    clay is .33, thetasMatric should be 44.593 */

    // Test bMatric
    EXPECT_GT(swrcp[2], 2.8); /* Value should always be greater than
    2.8 based upon complete consideration of potential range of sand and clay values */
    EXPECT_LT(swrcp[2], 18.8); /* Value should always be less
    than 18.8 based upon complete consideration of potential range of sand and clay values */
    EXPECT_DOUBLE_EQ(swrcp[2], 8.182); /* If sand is .33 and clay is .33,
    thetasMatric should be 8.182 */


    //--- Test `swcBulk_saturated`
    PDF_Saxton2006(&swc_sat, sand, clay, gravel, width);

    // The swcBulk_saturated should be greater than 0
    EXPECT_GT(swc_sat, 0.);
    // The swcBulk_saturated can't be greater than the width of the layer
    EXPECT_LT(swc_sat, width);



    //--- Test bad parameters: Cosby et al. 1984 PDF for Campbell's 1974 SWRC
    swrc_type = 1;
    pdf_type = 1;

    sand = 10. + 1./3.; // unrealistic but forces `bmatric` to become 0

    SWRC_PDF_estimate_parameters(
      swrc_type, pdf_type,
      swrcp,
      sand, clay, gravel
    );

    EXPECT_EQ(
      SWRC_check_parameters(swrc_type, swrcp),
      swFALSE
    );
  }


  // Test fatal failures of SWRC parameter checks
  TEST(SiteDeathTest, PDFs) {

    // inputs
    unsigned int swrc_type;
    RealD swrcp[SWRC_PARAM_NMAX];

    //--- Test unimplemented SWRC
    swrc_type = 255;

    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_check_parameters(swrc_type, swrcp),
      "@ generic.c LogError"
    );
  }


  // Test that `SW_SIT_init_run` fails on bad soil inputs
  TEST(SiteDeathTest, SoilParameters) {
    LyrIndex n1 = 0, n2 = 1, k = 2;
    RealD help;

    // Check error for bad bare-soil evaporation coefficient (should be [0-1])
    help = SW_Site.lyr[n1]->evap_coeff;
    SW_Site.lyr[n1]->evap_coeff = -0.5;
    EXPECT_DEATH_IF_SUPPORTED(SW_SIT_init_run(), "@ generic.c LogError");
    SW_Site.lyr[n1]->evap_coeff = help;

    // Check error for bad transpiration coefficient (should be [0-1])
    SW_Site.lyr[n2]->transp_coeff[k] = 1.5;
    EXPECT_DEATH_IF_SUPPORTED(SW_SIT_init_run(), "@ generic.c LogError");

    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }


  // Test that soil transpiration regions are derived well
  TEST(SWSiteTest, SoilTranspirationRegions) {
    /* Notes:
        - SW_Site.n_layers is base1
        - soil layer information in _TranspRgnBounds is base0
    */

    LyrIndex
      i, id,
      nRegions,
      prev_TranspRgnBounds[MAX_TRANSP_REGIONS] = {0};
    RealD
      soildepth;

    for (i = 0; i < MAX_TRANSP_REGIONS; ++i) {
      prev_TranspRgnBounds[i] = _TranspRgnBounds[i];
    }


    // Check that "default" values do not change region bounds
    nRegions = 3;
    RealD regionLowerBounds1[] = {20., 40., 100.};
    derive_soilRegions(nRegions, regionLowerBounds1);

    for (i = 0; i < nRegions; ++i) {
      // Quickly calculate soil depth for current region as output information
      soildepth = 0.;
      for (id = 0; id <= _TranspRgnBounds[i]; ++id) {
        soildepth += SW_Site.lyr[id]->width;
      }

      EXPECT_EQ(prev_TranspRgnBounds[i], _TranspRgnBounds[i]) <<
        "for transpiration region = " << i + 1 <<
        " at a soil depth of " << soildepth << " cm";
    }


    // Check that setting one region for all soil layers works
    nRegions = 1;
    RealD regionLowerBounds2[] = {100.};
    derive_soilRegions(nRegions, regionLowerBounds2);

    for (i = 0; i < nRegions; ++i) {
      EXPECT_EQ(SW_Site.n_layers - 1, _TranspRgnBounds[i]) <<
        "for a single transpiration region across all soil layers";
    }


    // Check that setting one region for one soil layer works
    nRegions = 1;
    RealD regionLowerBounds3[] = {SW_Site.lyr[0]->width};
    derive_soilRegions(nRegions, regionLowerBounds3);

    for (i = 0; i < nRegions; ++i) {
      EXPECT_EQ(0, _TranspRgnBounds[i]) <<
        "for a single transpiration region for the shallowest soil layer";
    }


    // Check that setting the maximal number of regions works
    nRegions = MAX_TRANSP_REGIONS;
    RealD *regionLowerBounds4 = new RealD[nRegions];
    // Example: one region each for the topmost soil layers
    soildepth = 0.;
    for (i = 0; i < nRegions; ++i) {
      soildepth += SW_Site.lyr[i]->width;
      regionLowerBounds4[i] = soildepth;
    }
    derive_soilRegions(nRegions, regionLowerBounds4);

    for (i = 0; i < nRegions; ++i) {
      EXPECT_EQ(i, _TranspRgnBounds[i]) <<
        "for transpiration region for the " << i + 1 << "-th soil layer";
    }

    delete[] regionLowerBounds4;

    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }

} // namespace
