#include "include/generic.h"        // for swTRUE
#include "include/SW_Control.h"     // for SW_CTL_main, SW_CTL_run_spinup
#include "include/SW_datastructs.h" // for SW_RUN
#include "include/SW_Main_lib.h"    // for sw_fail_on_error
#include "include/SW_Times.h"       // for Today, Time_get_lastdoy_y
#include "include/SW_VegProd.h"     // for SW_VPD_init_run, SW_VPD_deconstruct
#include "tests/gtests/sw_testhelpers.h" // for SpinUpFixtureTest
#include "gtest/gtest.h"                 // for Test, Message, TestPartResul...

#if defined(SW2_SpinupEvaluation)
#include "include/filefuncs.h"    // for OpenFile, CloseFile
#include "include/SW_Site.h"      // for SW_SIT_init_run
#include "include/SW_SoilWater.h" // for SW_SWC_init_run
#include "include/Times.h"        // for Today, Time_get_lastdoy_y
#include <stdio.h>                // for fprintf, fflush, snprintf
#endif


namespace {
// Test SpinUp with mode = 1 and scope > duration
TEST_F(SpinUpFixtureTest, Mode1WithScopeGreaterThanDuration) {
    int i;
    int const n = 4; // n = number of soil layers to test
    double *prevTemp = new double[n];
    double *prevMoist = new double[n];
    double *tempVals = NULL;

    const TimeInt startyr = SW_Run.ModelIn->startyr;
    const TimeInt endyr = SW_Run.ModelIn->endyr;

    const TimeInt n_years = endyr - startyr + 1;

    SW_VPD_init_run_mem(
        SW_Run.VegProdIn->veg_method,
        SW_Run.SiteIn->methodMaxDepthSoilTemperature,
        n_years,
        SW_Run.ModelIn->SW_SpinUp.duration,
        &SW_Run.VegProdSim,
        &LogInfo
    );
    SW_VPD_init_run_calc(&SW_Run, &LogInfo);
    sw_fail_on_error(&LogInfo);

    SW_Run.ModelIn->SW_SpinUp.mode = 1;
    SW_Run.ModelIn->SW_SpinUp.scope = 27;
    SW_Run.ModelIn->SW_SpinUp.duration = 3;

    // Turn on soil temperature simulations
    SW_Run.SiteIn->use_soil_temp = swTRUE;
    // Get initial soil temp and soil moisture levels
    for (i = 0; i < n; i++) {
        prevTemp[i] = SW_Run.RunIn.SoilRunIn.avgLyrTempInit[i];
        prevMoist[i] = SW_Run.SoilWatSim.swcBulk[Today][i];
    }
    // Turn on spinup flag
    SW_Run.ModelIn->SW_SpinUp.spinup = swTRUE;

    // Run the spinup
    SW_CTL_run_spinup(
        &SW_Domain, tempVals, &SW_Run_Template, &SW_Run, &LogInfo, &LogInfo
    );
    sw_fail_on_error(&LogInfo);

    memcpy(
        &SW_Run.RunIn.weathRunAllHist[0],
        &SW_Run_Template.RunIn.weathRunAllHist[0],
        sizeof(SW_WEATHER_HIST)
    );

    // Run (a short) simulation
    SW_Run.ModelIn->startyr = 1980;
    SW_Run.ModelIn->endyr = 1981;
    SW_Domain.SW_ConstInfo.ModelSim.doOutput = swFALSE;
    SW_CTL_run_single_site(
        SW_Run.ModelIn->startyr,
        SW_Run.ModelIn->endyr,
        &SW_Domain,
        &SW_Run_Template,
        &SW_Run,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (i = 0; i < n; i++) {
        // Check soil temp after spinup
        EXPECT_NE(prevTemp[i], SW_Run.SoilWatSim.avgLyrTemp[i])
            << "Soil temp error in test " << i << ": "
            << SW_Run.SoilWatSim.avgLyrTemp[i];

        // Check soil moisture after spinup
        EXPECT_NE(prevMoist[i], SW_Run.SoilWatSim.swcBulk[Today][i])
            << "Soil moisture error in test " << i << ": "
            << SW_Run.SoilWatSim.swcBulk[Today][i];
    }

    // Deallocate arrays
    delete[] prevTemp;
    delete[] prevMoist;

    SW_VPD_deconstruct(&SW_Run.VegProdSim);
}

// Test SpinUp with mode = 1 and scope = duration
TEST_F(SpinUpFixtureTest, Mode1WithScopeEqualToDuration) {
    int i;
    int const n = 4; // n = number of soil layers to test
    double *prevTemp = new double[n];
    double *prevMoist = new double[n];
    double *tempVals = NULL;

    const TimeInt startyr = SW_Run.ModelIn->startyr;
    const TimeInt endyr = SW_Run.ModelIn->endyr;

    const TimeInt n_years = endyr - startyr + 1;

    SW_VPD_init_run_mem(
        SW_Run.VegProdIn->veg_method,
        SW_Run.SiteIn->methodMaxDepthSoilTemperature,
        n_years,
        SW_Run.ModelIn->SW_SpinUp.duration,
        &SW_Run.VegProdSim,
        &LogInfo
    );
    SW_VPD_init_run_calc(&SW_Run, &LogInfo);
    sw_fail_on_error(&LogInfo);

    SW_Run.ModelIn->SW_SpinUp.mode = 1;
    SW_Run.ModelIn->SW_SpinUp.scope = 3;
    SW_Run.ModelIn->SW_SpinUp.duration = 3;

    // Turn on soil temperature simulations
    SW_Run.SiteIn->use_soil_temp = swTRUE;
    // Get initial soil temp and soil moisture levels
    for (i = 0; i < n; i++) {
        prevTemp[i] = SW_Run.RunIn.SoilRunIn.avgLyrTempInit[i];
        prevMoist[i] = SW_Run.SoilWatSim.swcBulk[Today][i];
    }
    // Turn on spinup flag
    SW_Run.ModelIn->SW_SpinUp.spinup = swTRUE;

    // Run the spinup
    SW_CTL_run_spinup(
        &SW_Domain, tempVals, &SW_Run_Template, &SW_Run, &LogInfo, &LogInfo
    );
    sw_fail_on_error(&LogInfo);

    // Run (a short) simulation
    SW_Run.ModelIn->startyr = 1980;
    SW_Run.ModelIn->endyr = 1981;
    SW_Domain.SW_ConstInfo.ModelSim.doOutput = swFALSE;
    SW_CTL_run_single_site(
        SW_Run.ModelIn->startyr,
        SW_Run.ModelIn->endyr,
        &SW_Domain,
        &SW_Run_Template,
        &SW_Run,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (i = 0; i < n; i++) {
        // Check soil temp after spinup
        EXPECT_NE(prevTemp[i], SW_Run.SoilWatSim.avgLyrTemp[i])
            << "Soil temp error in test " << i << ": "
            << SW_Run.SoilWatSim.avgLyrTemp[i];

        // Check soil moisture after spinup
        EXPECT_NE(prevMoist[i], SW_Run.SoilWatSim.swcBulk[Today][i])
            << "Soil moisture error in test " << i << ": "
            << SW_Run.SoilWatSim.swcBulk[Today][i];
    }

    // Deallocate arrays
    delete[] prevTemp;
    delete[] prevMoist;
    SW_VPD_deconstruct(&SW_Run.VegProdSim);
}

// Test SpinUp with mode = 1 and scope < duration
TEST_F(SpinUpFixtureTest, Mode1WithScopeLessThanDuration) {
    int i;
    int const n = 4; // n = number of soil layers to test
    double *prevTemp = new double[n];
    double *prevMoist = new double[n];
    double *tempVals = NULL;

    const TimeInt startyr = SW_Run.ModelIn->startyr;
    const TimeInt endyr = SW_Run.ModelIn->endyr;

    const TimeInt n_years = endyr - startyr + 1;

    SW_VPD_init_run_mem(
        SW_Run.VegProdIn->veg_method,
        SW_Run.SiteIn->methodMaxDepthSoilTemperature,
        n_years,
        SW_Run.ModelIn->SW_SpinUp.duration,
        &SW_Run.VegProdSim,
        &LogInfo
    );
    SW_VPD_init_run_calc(&SW_Run, &LogInfo);
    sw_fail_on_error(&LogInfo);

    SW_Run.ModelIn->SW_SpinUp.mode = 1;
    SW_Run.ModelIn->SW_SpinUp.scope = 1;
    SW_Run.ModelIn->SW_SpinUp.duration = 3;

    // Turn on soil temperature simulations
    SW_Run.SiteIn->use_soil_temp = swTRUE;
    // Get initial soil temp and soil moisture levels
    for (i = 0; i < n; i++) {
        prevTemp[i] = SW_Run.RunIn.SoilRunIn.avgLyrTempInit[i];
        prevMoist[i] = SW_Run.SoilWatSim.swcBulk[Today][i];
    }
    // Turn on spinup flag
    SW_Run.ModelIn->SW_SpinUp.spinup = swTRUE;

    // Run the spinup
    SW_CTL_run_spinup(
        &SW_Domain, tempVals, &SW_Run_Template, &SW_Run, &LogInfo, &LogInfo
    );
    sw_fail_on_error(&LogInfo);

    // Run (a short) simulation
    SW_Run.ModelIn->startyr = 1980;
    SW_Run.ModelIn->endyr = 1981;
    SW_Domain.SW_ConstInfo.ModelSim.doOutput = swFALSE;
    SW_CTL_run_single_site(
        SW_Run.ModelIn->startyr,
        SW_Run.ModelIn->endyr,
        &SW_Domain,
        &SW_Run_Template,
        &SW_Run,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (i = 0; i < n; i++) {
        // Check soil temp after spinup
        EXPECT_NE(prevTemp[i], SW_Run.SoilWatSim.avgLyrTemp[i])
            << "Soil temp error in test " << i << ": "
            << SW_Run.SoilWatSim.avgLyrTemp[i];

        // Check soil moisture after spinup
        EXPECT_NE(prevMoist[i], SW_Run.SoilWatSim.swcBulk[Today][i])
            << "Soil moisture error in test " << i << ": "
            << SW_Run.SoilWatSim.swcBulk[Today][i];
    }

    // Deallocate arrays
    delete[] prevTemp;
    delete[] prevMoist;

    SW_VPD_deconstruct(&SW_Run.VegProdSim);
}

// Test SpinUp with mode = 2 and scope > duration
TEST_F(SpinUpFixtureTest, Mode2WithScopeGreaterThanDuration) {
    int i;
    int const n = 4; // n = number of soil layers to test
    double *prevTemp = new double[n];
    double *prevMoist = new double[n];
    double *tempVals = NULL;

    const TimeInt startyr = SW_Run.ModelIn->startyr;
    const TimeInt endyr = SW_Run.ModelIn->endyr;

    const TimeInt n_years = endyr - startyr + 1;

    SW_VPD_init_run_mem(
        SW_Run.VegProdIn->veg_method,
        SW_Run.SiteIn->methodMaxDepthSoilTemperature,
        n_years,
        SW_Run.ModelIn->SW_SpinUp.duration,
        &SW_Run.VegProdSim,
        &LogInfo
    );
    SW_VPD_init_run_calc(&SW_Run, &LogInfo);
    sw_fail_on_error(&LogInfo);

    SW_Run.ModelIn->SW_SpinUp.mode = 2;
    SW_Run.ModelIn->SW_SpinUp.scope = 27;
    SW_Run.ModelIn->SW_SpinUp.duration = 3;

    // Turn on soil temperature simulations
    SW_Run.SiteIn->use_soil_temp = swTRUE;
    // Get initial soil temp and soil moisture levels
    for (i = 0; i < n; i++) {
        prevTemp[i] = SW_Run.RunIn.SoilRunIn.avgLyrTempInit[i];
        prevMoist[i] = SW_Run.SoilWatSim.swcBulk[Today][i];
    }
    // Turn on spinup flag
    SW_Run.ModelIn->SW_SpinUp.spinup = swTRUE;

    // Run the spinup
    SW_CTL_run_spinup(
        &SW_Domain, tempVals, &SW_Run_Template, &SW_Run, &LogInfo, &LogInfo
    );
    sw_fail_on_error(&LogInfo);

    // Run (a short) simulation
    SW_Run.ModelIn->startyr = 1980;
    SW_Run.ModelIn->endyr = 1981;
    SW_Domain.SW_ConstInfo.ModelSim.doOutput = swFALSE;
    SW_CTL_run_single_site(
        SW_Run.ModelIn->startyr,
        SW_Run.ModelIn->endyr,
        &SW_Domain,
        &SW_Run_Template,
        &SW_Run,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (i = 0; i < n; i++) {
        // Check soil temp after spinup
        EXPECT_NE(prevTemp[i], SW_Run.SoilWatSim.avgLyrTemp[i])
            << "Soil temp error in test " << i << ": "
            << SW_Run.SoilWatSim.avgLyrTemp[i];

        // Check soil moisture after spinup
        EXPECT_NE(prevMoist[i], SW_Run.SoilWatSim.swcBulk[Today][i])
            << "Soil moisture error in test " << i << ": "
            << SW_Run.SoilWatSim.swcBulk[Today][i];
    }

    // Deallocate arrays
    delete[] prevTemp;
    delete[] prevMoist;

    SW_VPD_deconstruct(&SW_Run.VegProdSim);
}

// Test SpinUp with mode = 2 and scope = duration
TEST_F(SpinUpFixtureTest, Mode2WithScopeEqualToDuration) {
    int i;
    int const n = 4; // n = number of soil layers to test
    double *prevTemp = new double[n];
    double *prevMoist = new double[n];
    double *tempVals = NULL;

    const TimeInt startyr = SW_Run.ModelIn->startyr;
    const TimeInt endyr = SW_Run.ModelIn->endyr;

    const TimeInt n_years = endyr - startyr + 1;

    SW_VPD_init_run_mem(
        SW_Run.VegProdIn->veg_method,
        SW_Run.SiteIn->methodMaxDepthSoilTemperature,
        n_years,
        SW_Run.ModelIn->SW_SpinUp.duration,
        &SW_Run.VegProdSim,
        &LogInfo
    );
    SW_VPD_init_run_calc(&SW_Run, &LogInfo);
    sw_fail_on_error(&LogInfo);

    SW_Run.ModelIn->SW_SpinUp.mode = 2;
    SW_Run.ModelIn->SW_SpinUp.scope = 3;
    SW_Run.ModelIn->SW_SpinUp.duration = 3;

    // Turn on soil temperature simulations
    SW_Run.SiteIn->use_soil_temp = swTRUE;
    // Get initial soil temp and soil moisture levels
    for (i = 0; i < n; i++) {
        prevTemp[i] = SW_Run.RunIn.SoilRunIn.avgLyrTempInit[i];
        prevMoist[i] = SW_Run.SoilWatSim.swcBulk[Today][i];
    }
    // Turn on spinup flag
    SW_Run.ModelIn->SW_SpinUp.spinup = swTRUE;

    // Run the spinup
    SW_CTL_run_spinup(
        &SW_Domain, tempVals, &SW_Run_Template, &SW_Run, &LogInfo, &LogInfo
    );
    sw_fail_on_error(&LogInfo);

    // Run (a short) simulation
    SW_Run.ModelIn->startyr = 1980;
    SW_Run.ModelIn->endyr = 1981;
    SW_Domain.SW_ConstInfo.ModelSim.doOutput = swFALSE;
    SW_CTL_run_single_site(
        SW_Run.ModelIn->startyr,
        SW_Run.ModelIn->endyr,
        &SW_Domain,
        &SW_Run_Template,
        &SW_Run,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (i = 0; i < n; i++) {
        // Check soil temp after spinup
        EXPECT_NE(prevTemp[i], SW_Run.SoilWatSim.avgLyrTemp[i])
            << "Soil temp error in test " << i << ": "
            << SW_Run.SoilWatSim.avgLyrTemp[i];

        // Check soil moisture after spinup
        EXPECT_NE(prevMoist[i], SW_Run.SoilWatSim.swcBulk[Today][i])
            << "Soil moisture error in test " << i << ": "
            << SW_Run.SoilWatSim.swcBulk[Today][i];
    }

    // Deallocate arrays
    delete[] prevTemp;
    delete[] prevMoist;

    SW_VPD_deconstruct(&SW_Run.VegProdSim);
}

// Test SpinUp with mode = 2 and scope < duration
TEST_F(SpinUpFixtureTest, Mode2WithScopeLessThanDuration) {
    int i;
    int const n = 4; // n = number of soil layers to test
    double *prevTemp = new double[n];
    double *prevMoist = new double[n];
    double *tempVals = NULL;

    const TimeInt startyr = SW_Run.ModelIn->startyr;
    const TimeInt endyr = SW_Run.ModelIn->endyr;

    const TimeInt n_years = endyr - startyr + 1;

    SW_VPD_init_run_mem(
        SW_Run.VegProdIn->veg_method,
        SW_Run.SiteIn->methodMaxDepthSoilTemperature,
        n_years,
        SW_Run.ModelIn->SW_SpinUp.duration,
        &SW_Run.VegProdSim,
        &LogInfo
    );

    SW_VPD_init_run_calc(&SW_Run, &LogInfo);
    sw_fail_on_error(&LogInfo);

    SW_Run.ModelIn->SW_SpinUp.mode = 2;
    SW_Run.ModelIn->SW_SpinUp.scope = 1;
    SW_Run.ModelIn->SW_SpinUp.duration = 3;

    // Turn on soil temperature simulations
    SW_Run.SiteIn->use_soil_temp = swTRUE;
    // Get initial soil temp and soil moisture levels
    for (i = 0; i < n; i++) {
        prevTemp[i] = SW_Run.RunIn.SoilRunIn.avgLyrTempInit[i];
        prevMoist[i] = SW_Run.SoilWatSim.swcBulk[Today][i];
    }
    // Turn on spinup flag
    SW_Run.ModelIn->SW_SpinUp.spinup = swTRUE;

    // Run the spinup
    SW_CTL_run_spinup(
        &SW_Domain, tempVals, &SW_Run_Template, &SW_Run, &LogInfo, &LogInfo
    );
    sw_fail_on_error(&LogInfo);

    // Run (a short) simulation
    SW_Run.ModelIn->startyr = 1980;
    SW_Run.ModelIn->endyr = 1981;
    SW_Domain.SW_ConstInfo.ModelSim.doOutput = swFALSE;
    SW_CTL_run_single_site(
        SW_Run.ModelIn->startyr,
        SW_Run.ModelIn->endyr,
        &SW_Domain,
        &SW_Run_Template,
        &SW_Run,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (i = 0; i < n; i++) {
        // Check soil temp after spinup
        EXPECT_NE(prevTemp[i], SW_Run.SoilWatSim.avgLyrTemp[i])
            << "Soil temp error in test " << i << ": "
            << SW_Run.SoilWatSim.avgLyrTemp[i];

        // Check soil moisture after spinup
        EXPECT_NE(prevMoist[i], SW_Run.SoilWatSim.swcBulk[Today][i])
            << "Soil moisture error in test " << i << ": "
            << SW_Run.SoilWatSim.swcBulk[Today][i];
    }

    // Deallocate arrays
    delete[] prevTemp;
    delete[] prevMoist;

    SW_VPD_deconstruct(&SW_Run.VegProdSim);
}

// Evaluate spinup

#ifdef SW2_SpinupEvaluation
// Run SOILWAT2 unit tests with flag
// ```
//   CPPFLAGS=-DSW2_SpinupEvaluation make test
//   bin/sw_test --gtest_filter=*SpinupEvaluation*
// ```
//
// Produce plots based on output generated above
// ```
//   Rscript tools/rscripts/Rscript__SW2_SpinupEvaluation.R
// ```

TEST_F(SpinUpFixtureTest, SpinupEvaluation) {
    const TimeInt n_years = SW_Domain.endyr - SW_Domain.startyr + 1;

    SW_RUN local_sw;
    LOG_INFO local_LogInfo;

    FILE *fp;
    char fname[FILENAME_MAX];
    int i;
    const int n = 8; // n = number of soil layers to test
    int k1;
    int k2;
    int k3;
    const int test_duration[6] = {0, 1, 3, 5, 10, 20};
    const double test_swcInit[4] = {0.5, 1, 15, 45};
    const double test_tsInit[5][8] = {
        {-2, -2, -2, -2, -2, -2, -2, -2},
        {0, 0, 0, 0, 0, 0, 0, 0},
        {-1, -1, -1, -1, 0, 0, 1, 1},
        {-2, -1.5, -1.25, -0.75, -0.5, 0.5, 1.5, 2},
        {2, 2, 2, 2, 2, 2, 2, 2}
    };
    double *tempVals = NULL;

    SW_VPD_init_run_calc(&SW_Run, &LogInfo);
    sw_fail_on_error(&LogInfo);

    const TimeInt endyr = SW_Run.ModelIn->startyr;
    bool dirExists;

    // Output file
    (void) snprintf(
        fname,
        sizeof fname,
        "%s%s",
        SW_Domain.SW_PathInputs.outputPrefix,
        "Table__SW2_SpinupEvaluation.csv"
    );
    dirExists = (bool) DirExists(SW_Domain.SW_PathInputs.outputPrefix);

    if (!dirExists) {
        MkDir(SW_Domain.SW_PathInputs.outputPrefix, &LogInfo);
        sw_fail_on_error(&LogInfo);
    }
    fp = OpenFile(fname, "w", &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Column names
    (void) fprintf(
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
                SW_RUN_deepCopy(
                    &SW_Run, &local_sw, swTRUE, n_years, &local_LogInfo
                );
                // exit test program if unexpected error
                sw_fail_on_error(&local_LogInfo);

                SW_VPD_init_run_mem(
                    local_sw.VegProdIn->veg_method,
                    local_sw.SiteIn->methodMaxDepthSoilTemperature,
                    n_years,
                    test_duration[k1],
                    &local_sw.VegProdSim,
                    &local_LogInfo
                );
                // exit test program if unexpected error
                sw_fail_on_error(&local_LogInfo);


                //--- k1: set spinup
                local_sw.ModelIn->SW_SpinUp.spinup = swTRUE;
                local_sw.ModelIn->SW_SpinUp.duration = test_duration[k1];
                local_sw.ModelIn->SW_SpinUp.mode = 1;
                local_sw.ModelIn->SW_SpinUp.scope = 1;


                //--- k2: set initial swc values
                local_sw.SiteIn->SWCInitVal = test_swcInit[k2];
                SW_SIT_init_run(
                    local_sw.VegProdIn,
                    local_sw.SiteIn,
                    &local_sw.RunIn.SiteRunIn,
                    &local_sw.SiteSim,
                    &local_sw.RunIn.SoilRunIn,
                    &SW_Run.VegProdIn->veg,
                    SW_Run.RunIn.SiteRunIn.n_layers,
                    &local_LogInfo
                );
                // exit test program if unexpected error
                sw_fail_on_error(&local_LogInfo);
                SW_SWC_init_run(
                    &local_sw.SoilWatSim,
                    &local_sw.SiteSim,
                    &local_sw.WeatherSim.temp_snow,
                    &local_sw.WeatherSim.snow_age,
                    SW_Run.RunIn.SiteRunIn.n_layers
                );


                //---k3: set initial soil temperature
                local_sw.SiteIn->use_soil_temp = swTRUE;
                for (i = 0; i < n; i++) {
                    local_sw.RunIn.SoilRunIn.avgLyrTempInit[i] =
                        test_tsInit[k3][i];
                }

                // Allocate and calculate CO2-effects
                SW_VPD_init_run_calc(&local_sw, &local_LogInfo);
                sw_fail_on_error(&local_LogInfo);

                // Print initial values
                for (i = 0; i < n; i++) {
                    (void) fprintf(
                        fp,
                        "init,%d,%f,%d,swc,%d,%f\n"
                        "init,%d,%f,%d,ts,%d,%f\n",
                        test_duration[k1],
                        test_swcInit[k2],
                        k3,
                        i,
                        local_sw.SoilWatSim.swcBulk[Today][i],
                        test_duration[k1],
                        test_swcInit[k2],
                        k3,
                        i,
                        local_sw.RunIn.SoilRunIn.avgLyrTempInit[i]
                    );
                }
                (void) fflush(fp);


                // Run the spinup
                if (test_duration[k1] > 0) {
                    SW_CTL_run_spinup(
                        &SW_Domain,
                        tempVals,
                        &SW_Run_Template,
                        &local_sw,
                        &local_LogInfo,
                        &local_LogInfo
                    );
                    sw_fail_on_error(&local_LogInfo);

                    // Print values after spinup
                    for (i = 0; i < n; i++) {
                        (void) fprintf(
                            fp,
                            "spinup,%d,%f,%d,swc,%d,%f\n"
                            "spinup,%d,%f,%d,ts,%d,%f\n",
                            test_duration[k1],
                            test_swcInit[k2],
                            k3,
                            i,
                            local_sw.SoilWatSim.swcBulk[Today][i],
                            test_duration[k1],
                            test_swcInit[k2],
                            k3,
                            i,
                            local_sw.SoilWatSim.avgLyrTemp[i]
                        );
                    }
                    (void) fflush(fp);
                }


                // Run (a short) simulation
                local_sw.ModelIn->endyr = local_sw.ModelIn->startyr;
                local_sw.ModelSim->doOutput = swFALSE;
                local_sw.ModelSim->lastdoy =
                    Time_get_lastdoy_y(local_sw.ModelIn->endyr);
                local_sw.ModelSim->year = local_sw.ModelIn->startyr;
                SW_CTL_run_single_site(
                    local_sw.ModelIn->startyr,
                    local_sw.ModelIn->endyr,
                    &SW_Domain,
                    &SW_Run_Template,
                    &local_sw,
                    &local_LogInfo
                );
                local_sw.ModelIn->endyr = endyr;

                // exit test program if unexpected error
                sw_fail_on_error(&local_LogInfo);

                // Print values after simulation
                for (i = 0; i < n; i++) {
                    (void) fprintf(
                        fp,
                        "srun,%d,%f,%d,swc,%d,%f\n"
                        "srun,%d,%f,%d,ts,%d,%f\n",
                        test_duration[k1],
                        test_swcInit[k2],
                        k3,
                        i,
                        local_sw.SoilWatSim.swcBulk[Today][i],
                        test_duration[k1],
                        test_swcInit[k2],
                        k3,
                        i,
                        local_sw.SoilWatSim.avgLyrTemp[i]
                    );
                }
                (void) fflush(fp);

                SW_CTL_clear_model(swTRUE, &local_sw);
            } // end of loop over test_tsInit
        } // end of loop over test_swcInit
    } // end of loop over test_duration

    CloseFile(&fp, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
}
#endif // end of SW2_SpinupEvaluation_Test

} // namespace
