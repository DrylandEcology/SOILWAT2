#include "include/generic.h"             // for RealD, swTRUE
#include "include/SW_Control.h"          // for SW_CTL_main, SW_CTL_run_spinup
#include "include/SW_Main_lib.h"         // for sw_fail_on_error
#include "include/SW_Times.h"            // for Today
#include "tests/gtests/sw_testhelpers.h" // for SpinUpTest
#include "gtest/gtest.h"                 // for Test, Message, TestPartResul...

#if defined(SW2_SpinupEvaluation)
#include "include/filefuncs.h"      // for OpenFile, CloseFile
#include "include/SW_datastructs.h" // for SW_RUN, LOG_INFO
#include "include/SW_Site.h"        // for SW_SIT_init_run
#include "include/SW_SoilWater.h"   // for SW_SWC_init_run
#include <stdio.h>                  // for fprintf, fflush, FILE
#include <string.h>                 // for strcat, strcpy
#endif


namespace {
// Test SpinUp with mode = 1 and scope > duration
TEST_F(SpinUpTest, Mode1WithScopeGreaterThanDuration) {
    int i, n = 4; // n = number of soil layers to test
    RealD *prevTemp = new double[n], *prevMoist = new double[n];

    SW_Run.Model.SW_SpinUp.mode = 1;
    SW_Run.Model.SW_SpinUp.scope = 27;
    SW_Run.Model.SW_SpinUp.duration = 3;

    // Turn on soil temperature simulations
    SW_Run.Site.use_soil_temp = swTRUE;
    // Get initial soil temp and soil moisture levels
    for (i = 0; i < n; i++) {
        prevTemp[i] = SW_Run.Site.avgLyrTempInit[i];
        prevMoist[i] = SW_Run.SoilWat.swcBulk[Today][i];
    }
    // Turn on spinup flag
    SW_Run.Model.SW_SpinUp.spinup = swTRUE;

    // Run the spinup
    SW_CTL_run_spinup(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo);

    // Run (a short) simulation
    SW_Run.Model.startyr = 1980;
    SW_Run.Model.endyr = 1981;
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (i = 0; i < n; i++) {
        // Check soil temp after spinup
        EXPECT_NE(prevTemp[i], SW_Run.SoilWat.avgLyrTemp[i])
            << "Soil temp error in test " << i << ": "
            << SW_Run.SoilWat.avgLyrTemp[i];

        // Check soil moisture after spinup
        EXPECT_NE(prevMoist[i], SW_Run.SoilWat.swcBulk[Today][i])
            << "Soil moisture error in test " << i << ": "
            << SW_Run.SoilWat.swcBulk[Today][i];
    }

    // Deallocate arrays
    delete[] prevTemp;
    delete[] prevMoist;
}

// Test SpinUp with mode = 1 and scope = duration
TEST_F(SpinUpTest, Mode1WithScopeEqualToDuration) {
    int i, n = 4; // n = number of soil layers to test
    RealD *prevTemp = new double[n], *prevMoist = new double[n];

    SW_Run.Model.SW_SpinUp.mode = 1;
    SW_Run.Model.SW_SpinUp.scope = 3;
    SW_Run.Model.SW_SpinUp.duration = 3;

    // Turn on soil temperature simulations
    SW_Run.Site.use_soil_temp = swTRUE;
    // Get initial soil temp and soil moisture levels
    for (i = 0; i < n; i++) {
        prevTemp[i] = SW_Run.Site.avgLyrTempInit[i];
        prevMoist[i] = SW_Run.SoilWat.swcBulk[Today][i];
    }
    // Turn on spinup flag
    SW_Run.Model.SW_SpinUp.spinup = swTRUE;

    // Run the spinup
    SW_CTL_run_spinup(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo);

    // Run (a short) simulation
    SW_Run.Model.startyr = 1980;
    SW_Run.Model.endyr = 1981;
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (i = 0; i < n; i++) {
        // Check soil temp after spinup
        EXPECT_NE(prevTemp[i], SW_Run.SoilWat.avgLyrTemp[i])
            << "Soil temp error in test " << i << ": "
            << SW_Run.SoilWat.avgLyrTemp[i];

        // Check soil moisture after spinup
        EXPECT_NE(prevMoist[i], SW_Run.SoilWat.swcBulk[Today][i])
            << "Soil moisture error in test " << i << ": "
            << SW_Run.SoilWat.swcBulk[Today][i];
    }

    // Deallocate arrays
    delete[] prevTemp;
    delete[] prevMoist;
}

// Test SpinUp with mode = 1 and scope < duration
TEST_F(SpinUpTest, Mode1WithScopeLessThanDuration) {
    int i, n = 4; // n = number of soil layers to test
    RealD *prevTemp = new double[n], *prevMoist = new double[n];

    SW_Run.Model.SW_SpinUp.mode = 1;
    SW_Run.Model.SW_SpinUp.scope = 1;
    SW_Run.Model.SW_SpinUp.duration = 3;

    // Turn on soil temperature simulations
    SW_Run.Site.use_soil_temp = swTRUE;
    // Get initial soil temp and soil moisture levels
    for (i = 0; i < n; i++) {
        prevTemp[i] = SW_Run.Site.avgLyrTempInit[i];
        prevMoist[i] = SW_Run.SoilWat.swcBulk[Today][i];
    }
    // Turn on spinup flag
    SW_Run.Model.SW_SpinUp.spinup = swTRUE;

    // Run the spinup
    SW_CTL_run_spinup(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo);

    // Run (a short) simulation
    SW_Run.Model.startyr = 1980;
    SW_Run.Model.endyr = 1981;
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (i = 0; i < n; i++) {
        // Check soil temp after spinup
        EXPECT_NE(prevTemp[i], SW_Run.SoilWat.avgLyrTemp[i])
            << "Soil temp error in test " << i << ": "
            << SW_Run.SoilWat.avgLyrTemp[i];

        // Check soil moisture after spinup
        EXPECT_NE(prevMoist[i], SW_Run.SoilWat.swcBulk[Today][i])
            << "Soil moisture error in test " << i << ": "
            << SW_Run.SoilWat.swcBulk[Today][i];
    }

    // Deallocate arrays
    delete[] prevTemp;
    delete[] prevMoist;
}

// Test SpinUp with mode = 2 and scope > duration
TEST_F(SpinUpTest, Mode2WithScopeGreaterThanDuration) {
    int i, n = 4; // n = number of soil layers to test
    RealD *prevTemp = new double[n], *prevMoist = new double[n];

    SW_Run.Model.SW_SpinUp.mode = 2;
    SW_Run.Model.SW_SpinUp.scope = 27;
    SW_Run.Model.SW_SpinUp.duration = 3;

    // Turn on soil temperature simulations
    SW_Run.Site.use_soil_temp = swTRUE;
    // Get initial soil temp and soil moisture levels
    for (i = 0; i < n; i++) {
        prevTemp[i] = SW_Run.Site.avgLyrTempInit[i];
        prevMoist[i] = SW_Run.SoilWat.swcBulk[Today][i];
    }
    // Turn on spinup flag
    SW_Run.Model.SW_SpinUp.spinup = swTRUE;

    // Run the spinup
    SW_CTL_run_spinup(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo);

    // Run (a short) simulation
    SW_Run.Model.startyr = 1980;
    SW_Run.Model.endyr = 1981;
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (i = 0; i < n; i++) {
        // Check soil temp after spinup
        EXPECT_NE(prevTemp[i], SW_Run.SoilWat.avgLyrTemp[i])
            << "Soil temp error in test " << i << ": "
            << SW_Run.SoilWat.avgLyrTemp[i];

        // Check soil moisture after spinup
        EXPECT_NE(prevMoist[i], SW_Run.SoilWat.swcBulk[Today][i])
            << "Soil moisture error in test " << i << ": "
            << SW_Run.SoilWat.swcBulk[Today][i];
    }

    // Deallocate arrays
    delete[] prevTemp;
    delete[] prevMoist;
}

// Test SpinUp with mode = 2 and scope = duration
TEST_F(SpinUpTest, Mode2WithScopeEqualToDuration) {
    int i, n = 4; // n = number of soil layers to test
    RealD *prevTemp = new double[n], *prevMoist = new double[n];

    SW_Run.Model.SW_SpinUp.mode = 2;
    SW_Run.Model.SW_SpinUp.scope = 3;
    SW_Run.Model.SW_SpinUp.duration = 3;

    // Turn on soil temperature simulations
    SW_Run.Site.use_soil_temp = swTRUE;
    // Get initial soil temp and soil moisture levels
    for (i = 0; i < n; i++) {
        prevTemp[i] = SW_Run.Site.avgLyrTempInit[i];
        prevMoist[i] = SW_Run.SoilWat.swcBulk[Today][i];
    }
    // Turn on spinup flag
    SW_Run.Model.SW_SpinUp.spinup = swTRUE;

    // Run the spinup
    SW_CTL_run_spinup(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo);

    // Run (a short) simulation
    SW_Run.Model.startyr = 1980;
    SW_Run.Model.endyr = 1981;
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (i = 0; i < n; i++) {
        // Check soil temp after spinup
        EXPECT_NE(prevTemp[i], SW_Run.SoilWat.avgLyrTemp[i])
            << "Soil temp error in test " << i << ": "
            << SW_Run.SoilWat.avgLyrTemp[i];

        // Check soil moisture after spinup
        EXPECT_NE(prevMoist[i], SW_Run.SoilWat.swcBulk[Today][i])
            << "Soil moisture error in test " << i << ": "
            << SW_Run.SoilWat.swcBulk[Today][i];
    }

    // Deallocate arrays
    delete[] prevTemp;
    delete[] prevMoist;
}

// Test SpinUp with mode = 2 and scope < duration
TEST_F(SpinUpTest, Mode2WithScopeLessThanDuration) {
    int i, n = 4; // n = number of soil layers to test
    RealD *prevTemp = new double[n], *prevMoist = new double[n];

    SW_Run.Model.SW_SpinUp.mode = 2;
    SW_Run.Model.SW_SpinUp.scope = 1;
    SW_Run.Model.SW_SpinUp.duration = 3;

    // Turn on soil temperature simulations
    SW_Run.Site.use_soil_temp = swTRUE;
    // Get initial soil temp and soil moisture levels
    for (i = 0; i < n; i++) {
        prevTemp[i] = SW_Run.Site.avgLyrTempInit[i];
        prevMoist[i] = SW_Run.SoilWat.swcBulk[Today][i];
    }
    // Turn on spinup flag
    SW_Run.Model.SW_SpinUp.spinup = swTRUE;

    // Run the spinup
    SW_CTL_run_spinup(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo);

    // Run (a short) simulation
    SW_Run.Model.startyr = 1980;
    SW_Run.Model.endyr = 1981;
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (i = 0; i < n; i++) {
        // Check soil temp after spinup
        EXPECT_NE(prevTemp[i], SW_Run.SoilWat.avgLyrTemp[i])
            << "Soil temp error in test " << i << ": "
            << SW_Run.SoilWat.avgLyrTemp[i];

        // Check soil moisture after spinup
        EXPECT_NE(prevMoist[i], SW_Run.SoilWat.swcBulk[Today][i])
            << "Soil moisture error in test " << i << ": "
            << SW_Run.SoilWat.swcBulk[Today][i];
    }

    // Deallocate arrays
    delete[] prevTemp;
    delete[] prevMoist;
}

// Evaluate spinup

#ifdef SW2_SpinupEvaluation
// Run SOILWAT2 unit tests with flag
// ```
//   CPPFLAGS=-DSW2_SpinupEvaluation make test && bin/sw_test
//   --gtest_filter=*SpinupEvaluation*
// ```
//
// Produce plots based on output generated above
// ```
//   Rscript tools/plot__SW2_SpinupEvaluation.R
// ```

TEST_F(SpinUpTest, SpinupEvaluation) {
    SW_RUN local_sw;
    LOG_INFO local_LogInfo;

    FILE *fp;
    char fname[FILENAME_MAX];
    int i, n = 8, // n = number of soil layers to test
        k1, test_duration[6] = {0, 1, 3, 5, 10, 20}, k2, k3;
    float test_swcInit[4] = {0.5, 1, 15, 45};
    float test_tsInit[5][8] = {
        {-2, -2, -2, -2, -2, -2, -2, -2},
        {0, 0, 0, 0, 0, 0, 0, 0},
        {-1, -1, -1, -1, 0, 0, 1, 1},
        {-2, -1.5, -1.25, -0.75, -0.5, 0.5, 1.5, 2},
        {2, 2, 2, 2, 2, 2, 2, 2}
    };


    // Output file
    strcpy(fname, SW_Domain.PathInfo.output_prefix);
    strcat(fname, "Table__SW2_SpinupEvaluation.csv");

    fp = OpenFile(fname, "w", &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Column names
    fprintf(
        fp,
        "stage,spinup_duration,swc_init,ts_init,variable,soil_layer,value"
        "\n"
    );


    for (k1 = 0; k1 < 6; k1++) {
        for (k2 = 0; k2 < 4; k2++) {
            for (k3 = 0; k3 < 5; k3++) {

                // Initialize logs and silence warn/error reporting
                sw_init_logs(NULL, &local_LogInfo);

                // deep copy of template
                SW_RUN_deepCopy(&SW_Run, &local_sw, &local_LogInfo);
                // exit test program if unexpected error
                sw_fail_on_error(&local_LogInfo);


                //--- k1: set spinup
                local_sw.Model.SW_SpinUp.spinup = swTRUE;
                local_sw.Model.SW_SpinUp.duration = test_duration[k1];
                local_sw.Model.SW_SpinUp.mode = 1;
                local_sw.Model.SW_SpinUp.scope = 1;


                //--- k2: set initial swc values
                local_sw.Site._SWCInitVal = test_swcInit[k2];
                SW_SIT_init_run(
                    &local_sw.VegProd, &local_sw.Site, &local_LogInfo
                );
                // exit test program if unexpected error
                sw_fail_on_error(&local_LogInfo);
                SW_SWC_init_run(
                    &local_sw.SoilWat,
                    &local_sw.Site,
                    &local_sw.Weather.temp_snow
                );


                //---k3: set initial soil temperature
                local_sw.Site.use_soil_temp = swTRUE;
                for (i = 0; i < n; i++) {
                    local_sw.Site.avgLyrTempInit[i] = test_tsInit[k3][i];
                }


                // Print initial values
                for (i = 0; i < n; i++) {
                    fprintf(
                        fp,
                        "init,%d,%f,%d,swc,%d,%f\n"
                        "init,%d,%f,%d,ts,%d,%f\n",
                        test_duration[k1],
                        test_swcInit[k2],
                        k3,
                        i,
                        local_sw.SoilWat.swcBulk[Today][i],
                        test_duration[k1],
                        test_swcInit[k2],
                        k3,
                        i,
                        local_sw.Site.avgLyrTempInit[i]
                    );
                }
                fflush(fp);


                // Run the spinup
                if (test_duration[k1] > 0) {
                    SW_CTL_run_spinup(&local_sw, &local_LogInfo);
                    sw_fail_on_error(&local_LogInfo);

                    // Print values after spinup
                    for (i = 0; i < n; i++) {
                        fprintf(
                            fp,
                            "spinup,%d,%f,%d,swc,%d,%f\n"
                            "spinup,%d,%f,%d,ts,%d,%f\n",
                            test_duration[k1],
                            test_swcInit[k2],
                            k3,
                            i,
                            local_sw.SoilWat.swcBulk[Today][i],
                            test_duration[k1],
                            test_swcInit[k2],
                            k3,
                            i,
                            local_sw.SoilWat.avgLyrTemp[i]
                        );
                    }
                    fflush(fp);
                }


                // Run (a short) simulation
                local_sw.Model.startyr = 1980;
                local_sw.Model.endyr = 1980;
                SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
                // exit test program if unexpected error
                sw_fail_on_error(&local_LogInfo);

                // Print values after simulation
                for (i = 0; i < n; i++) {
                    fprintf(
                        fp,
                        "srun,%d,%f,%d,swc,%d,%f\n"
                        "srun,%d,%f,%d,ts,%d,%f\n",
                        test_duration[k1],
                        test_swcInit[k2],
                        k3,
                        i,
                        local_sw.SoilWat.swcBulk[Today][i],
                        test_duration[k1],
                        test_swcInit[k2],
                        k3,
                        i,
                        local_sw.SoilWat.avgLyrTemp[i]
                    );
                }
                fflush(fp);

            } // end of loop over test_tsInit
        } // end of loop over test_swcInit
    } // end of loop over test_duration

    CloseFile(&fp, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
}
#endif // end of SW2_SpinupEvaluation_Test

} // namespace
