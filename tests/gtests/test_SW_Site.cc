#include <gmock/gmock.h>
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
#include "include/SW_Main_lib.h"

#include "tests/gtests/sw_testhelpers.h"

using ::testing::HasSubstr;

namespace {
  // List SWRC: PTFs
  const char *ns_ptfca2C1974[] = {
    "Campbell1974",
    "Cosby1984AndOthers", "Cosby1984"
  };
  const char *ns_ptfa2vG1980[] = {
    "vanGenuchten1980",
    // all PTFs
    "Rosetta3"
  };
  const char *ns_ptfc2vG1980[] = {
    "vanGenuchten1980"
    // PTFs implemented in SOILWAT2
  };
  const char *ns_ptfa2FXW[] = {
    "FXW"
    // all PTFs
    "neuroFX2021"
  };
  const char *ns_ptfc2FXW[] = {
    "FXW"
    // PTFs implemented in SOILWAT2
  };

  // Test pedotransfer functions
  TEST(SiteTest, SitePTFs) {
    LOG_INFO LogInfo;
    sw_init_logs(NULL, &LogInfo); // Initialize logs and silence warn/error reporting

    // inputs
    RealD
      swrcp[SWRC_PARAM_NMAX],
      sand = 0.33,
      clay = 0.33,
      gravel = 0.1,
      bdensity = 1.4;
    unsigned int swrc_type, k;


    //--- Matching PTF-SWRC pairs
    // (k starts at 1 because 0 holds the SWRC)

    swrc_type = encode_str2swrc((char *) ns_ptfca2C1974[0], &LogInfo);
    for (k = 1; k < length(ns_ptfca2C1974); k++) {
      SWRC_PTF_estimate_parameters(
        encode_str2ptf((char *) ns_ptfca2C1974[k]),
        swrcp,
        sand,
        clay,
        gravel,
        bdensity,
        &LogInfo
      );
      EXPECT_TRUE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    }

    swrc_type = encode_str2swrc((char *) ns_ptfc2vG1980[0], &LogInfo);
    for (k = 1; k < length(ns_ptfc2vG1980); k++) {
      SWRC_PTF_estimate_parameters(
        encode_str2ptf((char *) ns_ptfc2vG1980[k]),
        swrcp,
        sand,
        clay,
        gravel,
        bdensity,
        &LogInfo
      );
      EXPECT_TRUE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    }

    swrc_type = encode_str2swrc((char *) ns_ptfc2FXW[0], &LogInfo);
    for (k = 1; k < length(ns_ptfc2FXW); k++) {
      SWRC_PTF_estimate_parameters(
        encode_str2ptf((char *) ns_ptfc2FXW[k]),
        swrcp,
        sand,
        clay,
        gravel,
        bdensity,
        &LogInfo
      );
      EXPECT_TRUE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    }
  }


  // Test fatal failures of PTF estimation
  TEST(SiteTest, SitePTFsDeathTest) {
    LOG_INFO LogInfo;
    sw_init_logs(NULL, &LogInfo); // Initialize logs and silence warn/error reporting


    RealD
      swrcp[SWRC_PARAM_NMAX],
      sand = 0.33,
      clay = 0.33,
      gravel = 0.1,
      bdensity = 1.4;
    unsigned int ptf_type;


    //--- Test unimplemented PTF
    ptf_type = N_PTFs + 1;

    SWRC_PTF_estimate_parameters(
      ptf_type,
      swrcp,
      sand,
      clay,
      gravel,
      bdensity,
      &LogInfo
    );

    // Detect failure by error message
    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("PTF is not implemented in SOILWAT2"));

  }


  // Test PTF-SWRC pairings
  TEST(SiteTest, SitePTF2SWRC) {
    unsigned int k; // `length()` returns "unsigned long"

    for (k = 1; k < length(ns_ptfca2C1974); k++) {
      EXPECT_TRUE(
        (bool) check_SWRC_vs_PTF(
          (char *) ns_ptfca2C1974[0],
          (char *) ns_ptfca2C1974[k]
        )
      );

      EXPECT_FALSE(
        (bool) check_SWRC_vs_PTF(
          (char *) ns_ptfa2vG1980[0],
          (char *) ns_ptfca2C1974[k]
        )
      );

      EXPECT_FALSE(
        (bool) check_SWRC_vs_PTF(
          (char *) ns_ptfa2FXW[0],
          (char *) ns_ptfca2C1974[k]
        )
      );
    }

    for (k = 1; k < length(ns_ptfa2vG1980); k++) {
      EXPECT_FALSE(
        (bool) check_SWRC_vs_PTF(
          (char *) ns_ptfa2vG1980[0],
          (char *) ns_ptfa2vG1980[k]
        )
      );

      EXPECT_FALSE(
        (bool) check_SWRC_vs_PTF(
          (char *) ns_ptfca2C1974[0],
          (char *) ns_ptfa2vG1980[k]
        )
      );

      EXPECT_FALSE(
        (bool) check_SWRC_vs_PTF(
          (char *) ns_ptfa2FXW[0],
          (char *) ns_ptfa2vG1980[k]
        )
      );
    }


    for (k = 1; k < length(ns_ptfa2FXW); k++) {
      EXPECT_FALSE(
        (bool) check_SWRC_vs_PTF(
          (char *) ns_ptfa2FXW[0],
          (char *) ns_ptfa2FXW[k]
        )
      );

      EXPECT_FALSE(
        (bool) check_SWRC_vs_PTF(
          (char *) ns_ptfca2C1974[0],
          (char *) ns_ptfa2FXW[k]
        )
      );

      EXPECT_FALSE(
        (bool) check_SWRC_vs_PTF(
          (char *) ns_ptfa2vG1980[0],
          (char *) ns_ptfa2FXW[k]
        )
      );
    }

  }


  // Test fatal failures of SWRC parameter checks
  TEST(SiteTest, SiteSWRCpChecksDeathTest) {
    LOG_INFO LogInfo;
    sw_init_logs(NULL, &LogInfo); // Initialize logs and silence warn/error reporting

    // inputs
    RealD swrcp[SWRC_PARAM_NMAX];
    unsigned int swrc_type;


    //--- Test unimplemented SWRC
    swrc_type = N_SWRCs + 1;

    SWRC_check_parameters(swrc_type, swrcp, &LogInfo);

    // Detect failure by error message
    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("is not implemented"));
  }


  // Test nonfatal failures of SWRC parameter checks
  TEST(SiteTest, SiteSWRCpChecks) {
    LOG_INFO LogInfo;
    sw_init_logs(NULL, &LogInfo); // Initialize logs and silence warn/error reporting

    // inputs
    RealD
      swrcp[SWRC_PARAM_NMAX],
      tmp;
    unsigned int swrc_type;


    //--- SWRC: Campbell1974
    swrc_type = encode_str2swrc((char *) "Campbell1974", &LogInfo);
    memset(swrcp, 0., SWRC_PARAM_NMAX * sizeof(swrcp[0]));
    swrcp[0] = 24.2159;
    swrcp[1] = 0.4436;
    swrcp[2] = 10.3860;
    swrcp[3] = 14.14351;
    EXPECT_TRUE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));

    // Param1 = psi_sat (> 0)
    tmp = swrcp[0];
    swrcp[0] = -1.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    swrcp[0] = tmp;

    // Param2 = theta_sat (0-1)
    tmp = swrcp[1];
    swrcp[1] = -1.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    swrcp[1] = 1.5;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    swrcp[1] = tmp;

    // Param3 = beta (!= 0)
    tmp = swrcp[2];
    swrcp[2] = 0.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    swrcp[2] = tmp;



    //--- Fail SWRC: vanGenuchten1980
    swrc_type = encode_str2swrc((char *) "vanGenuchten1980", &LogInfo);
    memset(swrcp, 0., SWRC_PARAM_NMAX * sizeof(swrcp[0]));
    swrcp[0] = 0.1246;
    swrcp[1] = 0.4445;
    swrcp[2] = 0.0112;
    swrcp[3] = 1.2673;
    swrcp[4] = 7.7851;
    EXPECT_TRUE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));


    // Param1 = theta_res (0-1)
    tmp = swrcp[0];
    swrcp[0] = -1.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    swrcp[0] = 1.5;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    swrcp[0] = tmp;

    // Param2 = theta_sat (0-1 & > theta_res)
    tmp = swrcp[1];
    swrcp[1] = -1.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    swrcp[1] = 1.5;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    swrcp[1] = 0.5 * swrcp[0];
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    swrcp[1] = tmp;

    // Param3 = alpha (> 0)
    tmp = swrcp[2];
    swrcp[2] = 0.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    swrcp[2] = tmp;

    // Param4 = n (> 1)
    tmp = swrcp[3];
    swrcp[3] = 1.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    swrcp[3] = tmp;





    //--- Fail SWRC: FXW
    swrc_type = encode_str2swrc((char *) "FXW", &LogInfo);
    memset(swrcp, 0., SWRC_PARAM_NMAX * sizeof(swrcp[0]));
    swrcp[0] = 0.437461;
    swrcp[1] = 0.050757;
    swrcp[2] = 1.247689;
    swrcp[3] = 0.308681;
    swrcp[4] = 22.985379;
    swrcp[5] = 2.697338;
    EXPECT_TRUE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));


    // Param1 = theta_sat (0-1)
    tmp = swrcp[0];
    swrcp[0] = -1.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    swrcp[0] = 1.5;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    swrcp[0] = tmp;

    // Param2 = alpha (> 0)
    tmp = swrcp[1];
    swrcp[1] = 0.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    swrcp[1] = tmp;

    // Param3 = n (> 1)
    tmp = swrcp[2];
    swrcp[2] = 1.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    swrcp[2] = tmp;

    // Param4 = m (> 0)
    tmp = swrcp[3];
    swrcp[3] = 0.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    swrcp[3] = tmp;

    // Param5 = Ksat (> 0)
    tmp = swrcp[4];
    swrcp[4] = 0.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    swrcp[4] = tmp;

    // Param6 = L (> 0)
    tmp = swrcp[5];
    swrcp[5] = 0.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp, &LogInfo));
    swrcp[5] = tmp;
  }


  // Test 'PTF_RawlsBrakensiek1985'
  TEST(SiteTest, SitePTFRawlsBrakensiek1985) {
    LOG_INFO LogInfo;
    sw_init_logs(NULL, &LogInfo); // Initialize logs and silence warn/error reporting

    //declare mock INPUTS
    double
      theta_min,
      clay = 0.1,
      sand = 0.6,
      porosity = 0.4;
    int k1, k2, k3;

    //--- EXPECT SW_MISSING if soil texture is out of range
    // within range: sand [0.05, 0.7], clay [0.05, 0.6], porosity [0.1, 1[
    PTF_RawlsBrakensiek1985(&theta_min, 0., clay, porosity, &LogInfo);
    EXPECT_DOUBLE_EQ(theta_min, SW_MISSING);

    PTF_RawlsBrakensiek1985(&theta_min, 0.75, clay, porosity, &LogInfo);
    EXPECT_DOUBLE_EQ(theta_min, SW_MISSING);

    PTF_RawlsBrakensiek1985(&theta_min, sand, 0., porosity, &LogInfo);
    EXPECT_DOUBLE_EQ(theta_min, SW_MISSING);

    PTF_RawlsBrakensiek1985(&theta_min, sand, 0.65, porosity, &LogInfo);
    EXPECT_DOUBLE_EQ(theta_min, SW_MISSING);

    PTF_RawlsBrakensiek1985(&theta_min, sand, clay, 0., &LogInfo);
    EXPECT_DOUBLE_EQ(theta_min, SW_MISSING);

    PTF_RawlsBrakensiek1985(&theta_min, sand, clay, 1., &LogInfo);
    EXPECT_DOUBLE_EQ(theta_min, SW_MISSING);


    // Check that `theta_min` is reasonable over ranges of soil properties
    for (k1 = 0; k1 <= 5; k1++) {
      sand = 0.05 + (double) k1 / 5. * (0.7 - 0.05);

      for (k2 = 0; k2 <= 5; k2++) {
        clay = 0.05 + (double) k2 / 5. * (0.6 - 0.05);

        for (k3 = 0; k3 <= 5; k3++) {
          porosity = 0.1 + (double) k3 / 5. * (0.99 - 0.1);

          PTF_RawlsBrakensiek1985(&theta_min, sand, clay, porosity, &LogInfo);
          EXPECT_GE(theta_min, 0.);
          EXPECT_LT(theta_min, porosity);
        }
      }
    }

    // Expect theta_min = 0 if sand = 0.4, clay = 0.5, and porosity = 0.1
    PTF_RawlsBrakensiek1985(&theta_min, 0.4, 0.5, 0.1, &LogInfo);
    EXPECT_DOUBLE_EQ(theta_min, 0);
  }


  // Test that `SW_SIT_init_run` fails on bad soil inputs
  TEST_F(SiteFixtureTest, SiteSoilEvaporationParametersDeathTest) {

    // Check error for bad bare-soil evaporation coefficient (should be [0-1])

    SW_All.Site.evap_coeff[0] = -0.5;

    SW_SIT_init_run(&SW_All.VegProd, &SW_All.Site, &LogInfo);

    // Detect failure by error message
    EXPECT_THAT(LogInfo.errorMsg,
                HasSubstr("'bare-soil evaporation coefficient' has an invalid value"));
  }


  TEST_F(SiteFixtureTest, SiteSoilTranspirationParametersDeathTest) {

    // Check error for bad transpiration coefficient (should be [0-1])

    SW_All.Site.transp_coeff[SW_GRASS][1] = 1.5;
    SW_SIT_init_run(&SW_All.VegProd, &SW_All.Site, &LogInfo);

    // Detect failure by error message
    EXPECT_THAT(LogInfo.errorMsg,
                HasSubstr("'transpiration coefficient' has an invalid value"));
  }


  // Test that soil transpiration regions are derived well
  TEST_F(SiteFixtureTest, SiteSoilTranspirationRegions) {
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
      prev_TranspRgnBounds[i] = SW_All.Site._TranspRgnBounds[i];
    }


    // Check that "default" values do not change region bounds
    nRegions = 3;
    RealD regionLowerBounds1[] = {20., 40., 100.};
    derive_soilRegions(&SW_All.Site, nRegions, regionLowerBounds1, &LogInfo);

    for (i = 0; i < nRegions; ++i) {
      // Quickly calculate soil depth for current region as output information
      soildepth = 0.;
      for (id = 0; id <= SW_All.Site._TranspRgnBounds[i]; ++id) {
        soildepth += SW_All.Site.width[id];
      }

      EXPECT_EQ(prev_TranspRgnBounds[i], SW_All.Site._TranspRgnBounds[i]) <<
        "for transpiration region = " << i + 1 <<
        " at a soil depth of " << soildepth << " cm";
    }


    // Check that setting one region for all soil layers works
    nRegions = 1;
    RealD regionLowerBounds2[] = {100.};
    derive_soilRegions(&SW_All.Site, nRegions, regionLowerBounds2, &LogInfo);

    for (i = 0; i < nRegions; ++i) {
      EXPECT_EQ(SW_All.Site.n_layers - 1, SW_All.Site._TranspRgnBounds[i]) <<
        "for a single transpiration region across all soil layers";
    }


    // Check that setting one region for one soil layer works
    nRegions = 1;
    RealD regionLowerBounds3[] = {SW_All.Site.width[0]};
    derive_soilRegions(&SW_All.Site, nRegions, regionLowerBounds3, &LogInfo);

    for (i = 0; i < nRegions; ++i) {
      EXPECT_EQ(0, SW_All.Site._TranspRgnBounds[i]) <<
        "for a single transpiration region for the shallowest soil layer";
    }


    // Check that setting the maximal number of regions works
    nRegions = MAX_TRANSP_REGIONS;
    RealD *regionLowerBounds4 = new RealD[nRegions];
    // Example: one region each for the topmost soil layers
    soildepth = 0.;
    for (i = 0; i < nRegions; ++i) {
      soildepth += SW_All.Site.width[i];
      regionLowerBounds4[i] = soildepth;
    }
    derive_soilRegions(&SW_All.Site, nRegions, regionLowerBounds4, &LogInfo);

    for (i = 0; i < nRegions; ++i) {
      EXPECT_EQ(i, SW_All.Site._TranspRgnBounds[i]) <<
        "for transpiration region for the " << i + 1 << "-th soil layer";
    }

    delete[] regionLowerBounds4;
  }


  // Test bulk and matric soil density functionality
  TEST(SiteTest, SiteSoilDensity) {
    LOG_INFO LogInfo;
    sw_init_logs(NULL, &LogInfo); // Initialize logs and silence warn/error reporting

    double
      soildensity = 1.4,
      fcoarse = 0.1;


    // Check that matric density is zero if coarse fragments is 100%
    EXPECT_DOUBLE_EQ(
      calculate_soilMatricDensity(soildensity, 1., &LogInfo),
      0.
    );


    // Check that bulk and matric soil density are equal if no coarse fragments
    EXPECT_DOUBLE_EQ(
      calculate_soilBulkDensity(soildensity, 0.),
      calculate_soilMatricDensity(soildensity, 0., &LogInfo)
    );


    // Check that bulk and matric density calculations are inverse to each other
    EXPECT_DOUBLE_EQ(
      calculate_soilBulkDensity(
        calculate_soilMatricDensity(soildensity, fcoarse, &LogInfo),
        fcoarse
      ),
      soildensity
    );

    EXPECT_DOUBLE_EQ(
      calculate_soilMatricDensity(
        calculate_soilBulkDensity(soildensity, fcoarse),
        fcoarse,
        &LogInfo
      ),
      soildensity
    );


    // Check that bulk density is larger than matric density if coarse fragments
    EXPECT_GT(
      calculate_soilBulkDensity(soildensity, fcoarse),
      soildensity
    );
  }


  TEST_F(SiteFixtureTest, SiteSoilDensityTypes) {
    double fcoarse = 0.1;

    // Inputs represent matric density
    SW_All.Site.type_soilDensityInput = SW_MATRIC;
    SW_All.Site.fractionVolBulk_gravel[0] = fcoarse;
    SW_SIT_init_run(&SW_All.VegProd, &SW_All.Site, &LogInfo);

    EXPECT_GT(
      SW_All.Site.soilBulk_density[0],
      SW_All.Site.soilMatric_density[0]
    );


    // Inputs represent bulk density
    SW_All.Site.type_soilDensityInput = SW_BULK;
    SW_All.Site.fractionVolBulk_gravel[0] = fcoarse;
    SW_SIT_init_run(&SW_All.VegProd, &SW_All.Site, &LogInfo);

    EXPECT_GT(
      SW_All.Site.soilBulk_density[0],
      SW_All.Site.soilMatric_density[0]
    );
  }


  // Test that bulk and matric soil density fail
  TEST(SiteTest, SiteSoilDensityTooLowDeathTest) {
    LOG_INFO LogInfo;
    sw_init_logs(NULL, &LogInfo); // Initialize logs and silence warn/error reporting

    // Create an error if bulk density too low for coarse fragments
    calculate_soilMatricDensity(1.65, 0.7, &LogInfo);

    // Detect failure by error message
    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("is lower than expected"));
  }


  TEST_F(SiteFixtureTest, SiteSoilDensityMissingDeathTest) {
    // Create an error if type_soilDensityInput not implemented

    SW_All.Site.type_soilDensityInput = SW_MISSING;

    SW_SIT_init_run(&SW_All.VegProd, &SW_All.Site, &LogInfo);

    // Detect failure by error message
    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("Soil density type not recognized"));
  }
} // namespace
