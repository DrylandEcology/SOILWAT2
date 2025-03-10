#include "include/generic.h"             // for LOGERROR, LOGWARN
#include "include/SW_datastructs.h"      // for LOG_INFO, SW_SITE
#include "include/SW_Defines.h"          // for SW_MISSING, SWRC_PARAM_NMAX
#include "include/SW_Main_lib.h"         // for sw_fail_on_error, sw_init_logs
#include "include/SW_Site.h"             // for N_SWRCs, swrc2str, N_PTFs
#include "include/SW_SoilWater.h"        // for SWRC_SWCtoSWP, SWRC_SWPtoSWC
#include "tests/gtests/sw_testhelpers.h" // for tol9, length
#include "gmock/gmock.h"                 // for HasSubstr, MakePredicateFor...
#include "gtest/gtest.h"                 // for Test, Message, TestPartResul...
#include <sstream>                       // for basic_ostringstream, basic_...
#include <stdio.h>                       // for NULL
#include <string.h>                      // for memset
#include <string>                        // for allocator, char_traits

using ::testing::HasSubstr;

namespace {
// Test the 'SW_SoilWater' function 'SW_SWC_adjust_snow'

TEST(SoilWaterTest, SoilWaterSWCadjustSnow) {
    // setup variables
    const unsigned int doy = 1;
    double const temp_min = 0;
    double const temp_max = 10;
    double const ppt = 1;
    double rain = 1.5;
    double snow = 1.5;
    double snowmelt = 1.2;
    double temp_snow = 0;
    double snowpack[TWO_DAYS] = {0};

    SW_SITE SW_Site;

    SW_Site.TminAccu2 = 0;
    SW_Site.RmeltMax = 1;
    SW_Site.RmeltMin = 0;
    SW_Site.lambdasnow = .1;
    SW_Site.TmaxCrit = 1;


    SW_SWC_adjust_snow(
        &temp_snow,
        snowpack,
        &SW_Site,
        temp_min,
        temp_max,
        ppt,
        doy,
        &rain,
        &snow,
        &snowmelt
    );

    // when average temperature >= SW_Site.TminAccu2, we expect rain == ppt
    EXPECT_EQ(rain, 1);

    // when average temperature >= SW_Run.Site.TminAccu2, we expect snow == 0
    EXPECT_EQ(snow, 0);

    // when temp_snow <= SW_Run.Site.TmaxCrit, we expect snowmelt == 0
    EXPECT_EQ(snowmelt, 0);

    SW_Site.TminAccu2 = 6;

    SW_SWC_adjust_snow(
        &temp_snow,
        snowpack,
        &SW_Site,
        temp_min,
        temp_max,
        ppt,
        doy,
        &rain,
        &snow,
        &snowmelt
    );

    // when average temperature < SW_Site.TminAccu2, we expect rain == 0
    EXPECT_EQ(rain, 0);

    // when average temperature < SW_Run.Site.TminAccu2, we expect snow == ppt
    EXPECT_EQ(snow, 1);

    // when temp_snow > SW_Run.Site.TmaxCrit, we expect snowmelt == fmax(0,
    // *snowpack - *snowmelt )
    EXPECT_EQ(snowmelt, 0);
}

TEST(SoilWaterTest, SoilWaterSWCadjustSnow2) {
    const unsigned int doy = 1;
    double const temp_min = 0;
    double const temp_max = 22;
    double const ppt = 1;
    double rain = 1.5;
    double snow = 1.5;
    double snowmelt = 1.2;
    double temp_snow = 0;
    double snowpack[TWO_DAYS] = {0};

    SW_SITE SW_Site;

    SW_Site.TminAccu2 = 0;
    SW_Site.RmeltMax = 1;
    SW_Site.RmeltMin = 0;
    SW_Site.lambdasnow = .1;
    SW_Site.TmaxCrit = 1;


    SW_SWC_adjust_snow(
        &temp_snow,
        snowpack,
        &SW_Site,
        temp_min,
        temp_max,
        ppt,
        doy,
        &rain,
        &snow,
        &snowmelt
    );

    // when average temperature >= SW_Site.TminAccu2, we expect rain == ppt
    EXPECT_EQ(rain, 1);

    // when average temperature >= SW_Run.Site.TminAccu2, we expect snow == 0
    EXPECT_EQ(snow, 0);

    // when temp_snow > SW_Run.Site.TmaxCrit, we expect snowmelt == 0
    EXPECT_EQ(snowmelt, 0);
}

// Test the 'SW_SoilWater' functions 'SWRC_SWCtoSWP' and `SWRC_SWPtoSWC`
TEST(SoilWaterTest, SoilWaterTranslateBetweenSWCandSWP) {
    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    // set up mock variables
    unsigned int swrc_type;
    unsigned int ptf_type;
    unsigned int k;
    const int em = LOGERROR;
    double phi;
    double swcBulk;
    double swc_sat;
    double swc_fc;
    double swc_wp;
    double swp;
    double swrcp[SWRC_PARAM_NMAX];
    double const sand = 0.33;
    double const clay = 0.33;
    double const gravel = 0.2;
    double const bdensity = 1.4;
    double const width = 10.;
    // SWP values in [0, Inf[ but FXW maxes out at 6178.19079 bar
    double const swpsb[12] = {
        0., 0.001, 0.01, 0.026, 0.027, 0.33, 15., 30., 100., 300., 1000., 6178.
    };
    // SWP values in [fc, Inf[ but FXW maxes out at 6178.19079 bar
    double const swpsi[7] = {0.33, 15., 30., 100., 300., 1000., 6178.};

    std::ostringstream msg;


    // Loop over SWRCs
    for (swrc_type = 0; swrc_type < N_SWRCs; swrc_type++) {
        memset(swrcp, 0, SWRC_PARAM_NMAX * sizeof(swrcp[0]));

        // Find a suitable PTF to generate `SWRCp`
        for (ptf_type = 0;
             ptf_type < N_PTFs &&
             (check_SWRC_vs_PTF(
                  (char *) swrc2str[swrc_type], (char *) ptf2str[ptf_type]
              ) == 0u);
             ptf_type++) {
        }


        // Obtain SWRCp
        if (ptf_type < N_PTFs) {
            // PTF implemented in C: estimate parameters
            SWRC_PTF_estimate_parameters(
                ptf_type, swrcp, sand, clay, gravel, bdensity, &LogInfo
            );
            sw_fail_on_error(&LogInfo); // exit test program if unexpected error

        } else {
            // PTF not implemented in C: provide hard coded values
            if (Str_CompareI(
                    (char *) swrc2str[swrc_type], (char *) "vanGenuchten1980"
                ) == 0) {
                swrcp[0] = 0.11214750;
                swrcp[1] = 0.4213539;
                swrcp[2] = 0.007735474;
                swrcp[3] = 1.344678;
                swrcp[4] = 7.78506;

            } else if (Str_CompareI(
                           (char *) swrc2str[swrc_type], (char *) "FXW"
                       ) == 0) {
                swrcp[0] = 0.437461;
                swrcp[1] = 0.050757;
                swrcp[2] = 1.247689;
                swrcp[3] = 0.308681;
                swrcp[4] = 22.985379;
                swrcp[5] = 2.697338;

            } else {
                FAIL() << "No SWRC parameters available for "
                       << swrc2str[swrc_type];
            }
        }


        //------ Tests SWC -> SWP
        msg.str("");
        msg << "SWRC/PTF = " << swrc_type << "/" << ptf_type;

        // preferably we would print names instead of type codes, but
        // this leads to "global-buffer-overflow"
        // 0 bytes to the right of global variable 'ptf2str'
        // msg << "SWRC/PTF = " << swrc2str[swrc_type] << "/" <<
        // ptf2str[ptf_type];

        swc_sat =
            SWRC_SWPtoSWC(0., swrc_type, swrcp, gravel, width, em, &LogInfo);
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error
        swc_fc = SWRC_SWPtoSWC(
            1. / 3., swrc_type, swrcp, gravel, width, em, &LogInfo
        );
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error
        swc_wp =
            SWRC_SWPtoSWC(15., swrc_type, swrcp, gravel, width, em, &LogInfo);
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error


        // if swc = saturation, then we expect phi in [0, fc]
        // for instance, Campbell1974 goes to (theta_sat, swrcp[0]) instead of 0
        swp = SWRC_SWCtoSWP(
            swc_sat, swrc_type, swrcp, gravel, width, em, &LogInfo
        );
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error

        EXPECT_GE(swp, 0.) << msg.str();
        EXPECT_LT(swp, 1. / 3.) << msg.str();


        // if swc > field capacity, then we expect phi < 0.33 bar
        swcBulk = (swc_sat + swc_fc) / 2.;

        EXPECT_LT(
            SWRC_SWCtoSWP(
                swcBulk, swrc_type, swrcp, gravel, width, em, &LogInfo
            ),
            1. / 3.
        ) << msg.str();
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error

        // if swc = field capacity, then we expect phi == 0.33 bar
        EXPECT_NEAR(
            SWRC_SWCtoSWP(
                swc_fc, swrc_type, swrcp, gravel, width, em, &LogInfo
            ),
            1. / 3.,
            tol9
        ) << msg.str();
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error

        // if field capacity > swc > wilting point, then
        // we expect 15 bar > phi > 0.33 bar
        swcBulk = (swc_wp + swc_fc) / 2.;
        phi = SWRC_SWCtoSWP(
            swcBulk, swrc_type, swrcp, gravel, width, em, &LogInfo
        );
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error

        EXPECT_GT(phi, 1. / 3.) << msg.str();
        EXPECT_LT(phi, 15.) << msg.str();

        // if swc = wilting point, then we expect phi == 15 bar
        EXPECT_NEAR(
            SWRC_SWCtoSWP(
                swc_wp, swrc_type, swrcp, gravel, width, em, &LogInfo
            ),
            15.,
            tol9
        ) << msg.str();
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error

        // if swc < wilting point, then we expect phi > 15 bar
        swcBulk = SWRC_SWPtoSWC(
            2. * 15., swrc_type, swrcp, gravel, width, em, &LogInfo
        );
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error

        EXPECT_GT(
            SWRC_SWCtoSWP(
                swcBulk, swrc_type, swrcp, gravel, width, em, &LogInfo
            ),
            15.
        ) << msg.str();
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error


        //------ Tests SWP -> SWC
        // when fractionGravel is 1, we expect theta == 0
        EXPECT_EQ(
            SWRC_SWPtoSWC(15., swrc_type, swrcp, 1., width, em, &LogInfo), 0.
        ) << msg.str();
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error

        // when width is 0, we expect theta == 0
        EXPECT_EQ(
            SWRC_SWPtoSWC(15., swrc_type, swrcp, gravel, 0., em, &LogInfo), 0.
        ) << msg.str();
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error

        // check bounds of swc
        for (k = 0; k < sw_length(swpsb); k++) {
            swcBulk = SWRC_SWPtoSWC(
                swpsb[k], swrc_type, swrcp, gravel, width, em, &LogInfo
            );
            sw_fail_on_error(&LogInfo); // exit test program if unexpected error

            EXPECT_GE(swcBulk, 0.)
                << msg.str() << " at SWP = " << swpsb[k] << " bar";
            EXPECT_LE(swcBulk, width * (1. - gravel))
                << msg.str() << " at SWP = " << swpsb[k] << " bar";
        }


        //------ Tests that both SWP <-> SWC are inverse of each other
        // for phi at 0 (saturation) and phi in [fc, infinity]
        // but not necessarily if phi in ]0, fc[;
        // for instance, Campbell1974 is not inverse in ]0, swrcp[0][
        for (k = 0; k < sw_length(swpsi); k++) {
            swcBulk = SWRC_SWPtoSWC(
                swpsi[k], swrc_type, swrcp, gravel, width, em, &LogInfo
            );
            sw_fail_on_error(&LogInfo); // exit test program if unexpected error
            swp = SWRC_SWCtoSWP(
                swcBulk, swrc_type, swrcp, gravel, width, em, &LogInfo
            );
            sw_fail_on_error(&LogInfo); // exit test program if unexpected error

            EXPECT_NEAR(swp, swpsi[k], tol9)
                << msg.str() << " at SWP = " << swpsi[k] << " bar";

            EXPECT_NEAR(
                SWRC_SWPtoSWC(
                    swp, swrc_type, swrcp, gravel, width, em, &LogInfo
                ),
                swcBulk,
                tol9
            ) << msg.str()
              << " at SWC = " << swcBulk << " cm";

            sw_fail_on_error(&LogInfo); // exit test program if unexpected error
        }
    }
}

// Death Tests of 'SW_SoilWater' function 'SWRC_SWCtoSWP'
TEST(SoilWaterTest, SoilWaterSWCtoSWPDeathTest) {
    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    // set up mock variables
    double swrcp[SWRC_PARAM_NMAX];
    double const gravel = 0.1;
    double const width = 10.;

    unsigned int swrc_type;


    //--- we expect (non-)fatal errors in a few situations
    // (fatality depends on the error mode)

    //--- 1) Unimplemented SWRC
    swrc_type = N_SWRCs + 1;
    SWRC_SWCtoSWP(1., swrc_type, swrcp, gravel, width, LOGERROR, &LogInfo);
    // expect error: don't exit test program via `sw_fail_on_error(&LogInfo)`

    // Detect failure by error message
    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("is not implemented"));

    // Check that the same call but with a warning does not produce an error
    sw_init_logs(NULL, &LogInfo);
    EXPECT_DOUBLE_EQ(
        SWRC_SWCtoSWP(1., swrc_type, swrcp, gravel, width, LOGWARN, &LogInfo),
        SW_MISSING
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error


    for (swrc_type = 0; swrc_type < N_SWRCs; swrc_type++) {
        // --- 2a) fail if swc < 0: water content cannot be negative
        sw_init_logs(NULL, &LogInfo);
        SWRC_SWCtoSWP(-1., swrc_type, swrcp, gravel, width, LOGERROR, &LogInfo);
        // expect error: don't exit test program via
        // `sw_fail_on_error(&LogInfo)`

        // Detect failure by error message
        EXPECT_THAT(LogInfo.errorMsg, HasSubstr("invalid SWC"));
        sw_init_logs(NULL, &LogInfo);

        // Check that the same call but with a warning does not produce an error
        EXPECT_DOUBLE_EQ(
            SWRC_SWCtoSWP(
                -1., swrc_type, swrcp, gravel, width, LOGWARN, &LogInfo
            ),
            SW_MISSING
        );
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error


        // --- 2b) fail if gravel >= 1: gravel cannot be equal or larger than 1
        SWRC_SWCtoSWP(1., swrc_type, swrcp, 1., width, LOGERROR, &LogInfo);
        // expect error: don't exit test program via
        // `sw_fail_on_error(&LogInfo)`

        // Detect failure by error message
        EXPECT_THAT(LogInfo.errorMsg, HasSubstr("invalid gravel"));
        sw_init_logs(NULL, &LogInfo);

        // Check that the same call but with a warning does not produce an error
        EXPECT_DOUBLE_EQ(
            SWRC_SWCtoSWP(1., swrc_type, swrcp, 1., width, LOGWARN, &LogInfo),
            SW_MISSING
        );
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error


        // --- 2c) fail if soil layer width = 0: soil layers cannot be 0
        SWRC_SWCtoSWP(1., swrc_type, swrcp, gravel, 0., LOGERROR, &LogInfo);
        // expect error: don't exit test program via
        // `sw_fail_on_error(&LogInfo)`

        // Detect failure by error message
        EXPECT_THAT(LogInfo.errorMsg, HasSubstr("invalid layer width"));
        sw_init_logs(NULL, &LogInfo);

        // Check that the same call but with a warning does not produce an error
        EXPECT_DOUBLE_EQ(
            SWRC_SWCtoSWP(1., swrc_type, swrcp, gravel, 0., LOGWARN, &LogInfo),
            SW_MISSING
        );
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    }

    // --- *) fail if (theta - theta_res) < 0 (specific to vanGenuchten1980)
    // note: this case is normally prevented due to SWC checks
    swrc_type = encode_str2swrc((char *) "vanGenuchten1980", &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    memset(swrcp, 0, SWRC_PARAM_NMAX * sizeof(swrcp[0]));
    swrcp[0] = 0.1246;
    swrcp[1] = 0.4445;
    swrcp[2] = 0.0112;
    swrcp[3] = 1.2673;
    swrcp[4] = 7.78506;

    SWRC_SWCtoSWP(
        0.99 * swrcp[0], swrc_type, swrcp, gravel, width, LOGERROR, &LogInfo
    );
    // expect error: don't exit test program via `sw_fail_on_error(&LogInfo)`

    // Detect failure by error message
    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("invalid value of\n\ttheta"));
    sw_init_logs(NULL, &LogInfo);

    EXPECT_DOUBLE_EQ(
        SWRC_SWCtoSWP(
            0.99 * swrcp[0], swrc_type, swrcp, gravel, width, LOGWARN, &LogInfo
        ),
        SW_MISSING
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
}

// Death Tests of 'SW_SoilWater' function 'SWRC_SWPtoSWC'
TEST(SoilWaterTest, SoilWaterSWPtoSWCDeathTest) {
    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    // set up mock variables
    double swrcp[SWRC_PARAM_NMAX];
    double const gravel = 0.1;
    double const width = 10.;

    unsigned int swrc_type;


    //--- we expect (non-)fatal errors in two situations
    // (fatality depends on the error mode)

    //--- 1) Unimplemented SWRC
    swrc_type = N_SWRCs + 1;
    SWRC_SWPtoSWC(15., swrc_type, swrcp, gravel, width, LOGERROR, &LogInfo);
    // expect error: don't exit test program via `sw_fail_on_error(&LogInfo)`

    // Detect failure by error message
    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("is not implemented"));

    // Check that the same call but with a warning does not produce an error
    sw_init_logs(NULL, &LogInfo);
    EXPECT_DOUBLE_EQ(
        SWRC_SWPtoSWC(15., swrc_type, swrcp, gravel, width, LOGWARN, &LogInfo),
        SW_MISSING
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // --- 2) swp < 0: water content cannot be negative (any SWRC)
    for (swrc_type = 0; swrc_type < N_SWRCs; swrc_type++) {
        SWRC_SWPtoSWC(-1., swrc_type, swrcp, gravel, width, LOGERROR, &LogInfo);
        // expect error: don't exit test program via
        // `sw_fail_on_error(&LogInfo)`

        // Detect failure by error message
        EXPECT_THAT(LogInfo.errorMsg, HasSubstr("invalid SWP"));

        // Check that the same call but with a warning does not produce an error
        sw_init_logs(NULL, &LogInfo);
        EXPECT_DOUBLE_EQ(
            SWRC_SWPtoSWC(
                -1., swrc_type, swrcp, gravel, width, LOGWARN, &LogInfo
            ),
            SW_MISSING
        );
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    }
}
} // namespace
