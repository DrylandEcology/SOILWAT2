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
  // List SWRC: PDFs
  const char *ns_pdfca2C1974[] = {
    "Campbell1974",
    "Cosby1984AndOthers", "Cosby1984"
  };
  const char *ns_pdfa2vG1980[] = {
    "vanGenuchten1980",
    // all PDFs
    "Rosetta3"
  };
  const char *ns_pdfc2vG1980[] = {
    "vanGenuchten1980"
    // PDFs implemented in C
  };


  // Test pedotransfer functions
  TEST(SiteTest, PDFs) {
    // inputs
    RealD
      swrcp[SWRC_PARAM_NMAX],
      sand = 0.33,
      clay = 0.33,
      gravel = 0.1;
    unsigned int swrc_type, k;


    //--- Matching PDF-SWRC pairs
    // (k starts at 1 because 0 holds the SWRC)

    swrc_type = encode_str2swrc((char *) ns_pdfca2C1974[0]);
    for (k = 1; k < length(ns_pdfca2C1974); k++) {
      SWRC_PDF_estimate_parameters(
        encode_str2pdf((char *) ns_pdfca2C1974[k]),
        swrcp,
        sand, clay, gravel
      );
      EXPECT_TRUE((bool) SWRC_check_parameters(swrc_type, swrcp));
    }

    swrc_type = encode_str2swrc((char *) ns_pdfc2vG1980[0]);
    for (k = 1; k < length(ns_pdfc2vG1980); k++) {
      SWRC_PDF_estimate_parameters(
        encode_str2pdf((char *) ns_pdfc2vG1980[k]),
        swrcp,
        sand, clay, gravel
      );
      EXPECT_TRUE((bool) SWRC_check_parameters(swrc_type, swrcp));
    }
  }


  // Test fatal failures of PDF estimation
  TEST(SiteDeathTest, PDFs) {

    RealD
      swrcp[SWRC_PARAM_NMAX],
      sand = 0.33,
      clay = 0.33,
      gravel = 0.1;
    unsigned int pdf_type;


    //--- Test unimplemented PDF
    pdf_type = N_PDFs + 1;

    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_PDF_estimate_parameters(pdf_type, swrcp, sand, clay, gravel),
      "@ generic.c LogError"
    );
  }


  // Test PDF-SWRC pairings
  TEST(SiteTest, PDF2SWRC) {
    unsigned int k; // `length()` returns "unsigned long"

    // Matching/incorrect PDF-SWRC pairs
    // (k starts at 1 because 0 holds the SWRC)
    for (k = 0; k < N_SWRCs; k++) {
      EXPECT_TRUE(
        (bool) check_SWRC_vs_PDF((char *) swrc2str[k], (char *) "NoPDF", swTRUE)
      );
    }

    for (k = 1; k < length(ns_pdfca2C1974); k++) {
      EXPECT_TRUE(
        (bool) check_SWRC_vs_PDF(
          (char *) ns_pdfca2C1974[0],
          (char *) ns_pdfca2C1974[k],
          swTRUE
        )
      );

      EXPECT_FALSE(
        (bool) check_SWRC_vs_PDF(
          (char *) ns_pdfa2vG1980[0],
          (char *) ns_pdfca2C1974[k],
          swTRUE
        )
      );
    }

    for (k = 1; k < length(ns_pdfa2vG1980); k++) {
      EXPECT_TRUE(
        (bool) check_SWRC_vs_PDF(
          (char *) ns_pdfa2vG1980[0],
          (char *) ns_pdfa2vG1980[k],
          swFALSE
        )
      );

      EXPECT_FALSE(
        (bool) check_SWRC_vs_PDF(
          (char *) ns_pdfca2C1974[0],
          (char *) ns_pdfa2vG1980[k],
          swFALSE
        )
      );
    }
  }


  // Test fatal failures of SWRC parameter checks
  TEST(SiteDeathTest, SWRCpChecks) {

    // inputs
    RealD swrcp[SWRC_PARAM_NMAX];
    unsigned int swrc_type;


    //--- Test unimplemented SWRC
    swrc_type = N_SWRCs + 1;

    EXPECT_DEATH_IF_SUPPORTED(
      SWRC_check_parameters(swrc_type, swrcp),
      "@ generic.c LogError"
    );
  }


  // Test nonfatal failures of SWRC parameter checks
  TEST(SiteTest, SWRCpChecks) {

    // inputs
    RealD
      swrcp[SWRC_PARAM_NMAX],
      tmp;
    unsigned int swrc_type;


    //--- SWRC: Campbell1974
    swrc_type = encode_str2swrc((char *) "Campbell1974");
    memset(swrcp, 0., SWRC_PARAM_NMAX * sizeof(swrcp[0]));
    swrcp[0] = 24.2159;
    swrcp[1] = 0.4436;
    swrcp[2] = 10.3860;
    EXPECT_TRUE((bool) SWRC_check_parameters(swrc_type, swrcp));

    // Param1 = psi_sat (> 0)
    tmp = swrcp[0];
    swrcp[0] = -1.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp));
    swrcp[0] = tmp;

    // Param2 = theta_sat (0-1)
    tmp = swrcp[1];
    swrcp[1] = -1.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp));
    swrcp[1] = 1.5;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp));
    swrcp[1] = tmp;

    // Param3 = beta (!= 0)
    tmp = swrcp[2];
    swrcp[2] = 0.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp));
    swrcp[2] = tmp;



    //--- Fail SWRC: vanGenuchten1980
    swrc_type = encode_str2swrc((char *) "vanGenuchten1980");
    memset(swrcp, 0., SWRC_PARAM_NMAX * sizeof(swrcp[0]));
    swrcp[0] = 0.1246;
    swrcp[1] = 0.4445;
    swrcp[2] = 0.0112;
    swrcp[3] = 1.2673;
    EXPECT_TRUE((bool) SWRC_check_parameters(swrc_type, swrcp));


    // Param1 = theta_res (0-1)
    tmp = swrcp[0];
    swrcp[0] = -1.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp));
    swrcp[0] = 1.5;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp));
    swrcp[0] = tmp;

    // Param2 = theta_sat (0-1 & > theta_res)
    tmp = swrcp[1];
    swrcp[1] = -1.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp));
    swrcp[1] = 1.5;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp));
    swrcp[1] = 0.5 * swrcp[0];
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp));
    swrcp[1] = tmp;

    // Param3 = alpha (> 0)
    tmp = swrcp[2];
    swrcp[2] = 0.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp));
    swrcp[2] = tmp;

    // Param4 = n (> 1)
    tmp = swrcp[3];
    swrcp[3] = 1.;
    EXPECT_FALSE((bool) SWRC_check_parameters(swrc_type, swrcp));
    swrcp[3] = tmp;
  }


  // Test 'PDF_RawlsBrakensiek1985'
  TEST(SiteTest, PDFRawlsBrakensiek1985) {
    //declare mock INPUTS
    double
      theta_min,
      clay = 0.1,
      sand = 0.6,
      porosity = 0.4;
    int k1, k2, k3;

    //--- EXPECT SW_MISSING if soil texture is out of range
    // within range: sand [0.05, 0.7], clay [0.05, 0.6], porosity [0.1, 1[
    PDF_RawlsBrakensiek1985(&theta_min, 0., clay, porosity);
    EXPECT_DOUBLE_EQ(theta_min, SW_MISSING);

    PDF_RawlsBrakensiek1985(&theta_min, 0.75, clay, porosity);
    EXPECT_DOUBLE_EQ(theta_min, SW_MISSING);

    PDF_RawlsBrakensiek1985(&theta_min, sand, 0., porosity);
    EXPECT_DOUBLE_EQ(theta_min, SW_MISSING);

    PDF_RawlsBrakensiek1985(&theta_min, sand, 0.65, porosity);
    EXPECT_DOUBLE_EQ(theta_min, SW_MISSING);

    PDF_RawlsBrakensiek1985(&theta_min, sand, clay, 0.);
    EXPECT_DOUBLE_EQ(theta_min, SW_MISSING);

    PDF_RawlsBrakensiek1985(&theta_min, sand, clay, 1.);
    EXPECT_DOUBLE_EQ(theta_min, SW_MISSING);


    // Check that `theta_min` is reasonable over ranges of soil properties
    for (k1 = 0; k1 <= 5; k1++) {
      sand = 0.05 + (double) k1 / 5. * (0.7 - 0.05);

      for (k2 = 0; k2 <= 5; k2++) {
        clay = 0.05 + (double) k2 / 5. * (0.6 - 0.05);

        for (k3 = 0; k3 <= 5; k3++) {
          porosity = 0.1 + (double) k3 / 5. * (0.99 - 0.1);

          PDF_RawlsBrakensiek1985(&theta_min, sand, clay, porosity);
          EXPECT_GE(theta_min, 0.);
          EXPECT_LT(theta_min, porosity);
        }
      }
    }

    // Expect theta_min = 0 if sand = 0.4, clay = 0.5, and porosity = 0.1
    PDF_RawlsBrakensiek1985(&theta_min, 0.4, 0.5, 0.1);
    EXPECT_DOUBLE_EQ(theta_min, 0);
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
