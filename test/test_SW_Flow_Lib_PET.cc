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

#include "../SW_Flow_lib_PET.h"

#include "sw_testhelpers.h"

extern char output_prefix[FILENAME_MAX];


namespace
{
  // Test solar position
  TEST(SW2_SolarPosition_Test, solar_position)
  {
    double declin, reldist, lat,
      six_hours = 6. * swPI / 12.,
      // Min/max solar declination = angle of Earth's axial tilt/obliquity
      //   value for 2020 based on Astronomical Almanac 2010
      declin_max = 23.43668 * deg_to_rad,
      declin_min = -declin_max,
      // Min/max relative sun-earth distance
      //   values based on Astronomical Almanac 2010
      reldist_max = 1.01671,
      reldist_min = 0.98329;

    int i,
      // Dates of equinoxes and solstices (day of nonleap year):
      //   - March equinox (March 19-21)
      //   - June solstice (Jun 20-22)
      //   - September equinox (Sep 21-24)
      //   - December solistice (Dec 20-23)
      doy_Mar_equinox[2] = {79, 81},
      doy_Sep_equinox[2] = {264, 266},
      doy_Jun_solstice[2] = {171, 173},
      doy_Dec_solstice[2] = {354, 357},
      // Dates of perihelion and aphelion
      doy_perihelion[2] = {2, 5},
      doy_aphelion[2] = {184, 187};


    for (i = 1; i <= 366; i++) {
      //------ Relative sun-earth distance
      reldist = sqrt(1. / sun_earth_distance_squaredinverse(i));

      if (i >= doy_perihelion[0] && i <= doy_perihelion[1]) {
        // Test: sun-earth distance reaches min c. 14 days after Dec solstice
        EXPECT_NEAR(reldist, reldist_min, tol3) << "doy = " << i;

      } else if (i >= doy_aphelion[0] && i <= doy_aphelion[1]) {
        // Test: sun-earth distance reaches max c. 14 days after Jun solstice
        EXPECT_NEAR(reldist, reldist_max, tol3) << "doy = " << i;

      } else {
        EXPECT_LE(reldist, reldist_max + tol3) << "doy = " << i;
        EXPECT_GE(reldist, reldist_min - tol3) << "doy = " << i;
      }


      //------ Solar declination
      declin = solar_declination(i);

      // Test: solar declination changes sign on equinox
      if (i <= doy_Mar_equinox[0] || i > doy_Sep_equinox[1]) {
        EXPECT_LT(declin, 0.) << "doy = " << i;

      } else if (i > doy_Mar_equinox[1] && i <= doy_Sep_equinox[0]) {
        EXPECT_GT(declin, 0.) << "doy = " << i;
      }

      // Test: solar declination reaches max/min value on solstice
      if (i >= doy_Jun_solstice[0] && i <= doy_Jun_solstice[1]) {
        EXPECT_NEAR(declin, declin_max, tol3) << "doy = " << i;

      } else if (i >= doy_Dec_solstice[0] && i <= doy_Dec_solstice[1]) {
        EXPECT_NEAR(declin, declin_min, tol3) << "doy = " << i;

      } else {
        EXPECT_LE(declin, declin_max + tol3) << "doy = " << i;
        EXPECT_GE(declin, declin_min - tol3) << "doy = " << i;
      }


      //------ Sunset hour angle on horizontal surface
      // Test: every day has six hour of possible sunshine on equator
      EXPECT_NEAR(
        sunset_hourangle(0., declin),
        six_hours,
        tol6
      ) << "doy = " << i;
    }


    // Sunset hour angle on horizontal surface
    // Test: every location has six hours of possible sunshine on equinoxes
    for (i = 0; i <= 10; i++) {
      lat = (-90. + 180. * (i - 0.) / 10.) * deg_to_rad;
      EXPECT_NEAR(
        sunset_hourangle(lat, 0.),
        six_hours,
        tol3
      ) << "lat = " << lat;
    }
  }


  // Test solar declination and sunset hour angle
  TEST(SW2_PET_Test, solar_metrics)
  {
    double declin,
      declin_max = 0.4094, // should rather be 0.409 = (23+26/60)*pi/180
      declin_min = -declin_max;
    int i,
      doy_Mar_equinox = 80,
      doy_Sep_equinox = 266,
      doy_Jun_solstice = 173,
      doy_Dec_solstice = 355;
    double ahou,
      rlat_equator = 0.,
      rlat,
      six_hours = 6 * swPI / 12;


    // Loop through each day of year
    for (i = 1; i <= 365; i++) {
      // Solar declination
      declin = solar_declination(i);

      // Equinox: sign changes
      if (i <= doy_Mar_equinox || i > doy_Sep_equinox) {
        EXPECT_LT(declin, 0) << "doy = " << i;

      } else if (i > doy_Mar_equinox && i <= doy_Sep_equinox) {
        EXPECT_GT(declin, 0) << "doy = " << i;
      }

      // Solstice: max/min
      if (i == doy_Jun_solstice) {
        EXPECT_NEAR(declin, declin_max, tol3) << "doy = " << i;
      }
      if (i == doy_Dec_solstice) {
        EXPECT_NEAR(declin, declin_min, tol3) << "doy = " << i;
      }

      EXPECT_LE(declin, declin_max) << "doy = " << i;
      EXPECT_GE(declin, declin_min) << "doy = " << i;
  // Test extraterrestrial solar radiation
  //   Comparison against examples by Duffie & Beckman 2013 are expected to
  //   deviate in value, but show similar patterns, because equations for
  //   (i) sun-earth distance equation and (ii) solar declination differ
  TEST(SW2_SolarRadiation_Test, extraterrestrial)
  {
    unsigned int k1, k2, doy;
    double
      lat,
      lat_Madison_WI = 43. * deg_to_rad, // Duffie & Beckman 2013: Ex 1.6.1
      lat_StLouis_MO = 38.6 * deg_to_rad, // Duffie & Beckman 2013: Ex 2.11.1
      int_cos_theta[2], int_sin_beta[2],
      H_o[2],
      res_ratio;

    // Duffie & Beckman 2013: Table 1.10.1
    unsigned int doys_Table1_6_1[12] = {
      17, 47, 75, 105, 135, 162, 198, 228, 258, 288, 318, 344
    };
    double
      lats_Table1_10_1[9] = {85., 45., 30., 15., 0., -10., -45., -60., -90.},
      // values off at high polar latitudes
      // during shifts between permanent sun and night
      //   * lat = +85: Mar = 2.2, Sep = 6.4
      //   * lat = -90: Mar = 6.2, Sep = 1.4, Oct = 20.4
      H_oh_Table1_10_1[9][12] = {
        {0.0, 0.0, NAN, 19.2, 37.0, 44.7, 41.0, 26.4, NAN, 0.0, 0.0, 0.0},
        {12.2, 17.4, 25.1, 33.2, 39.2, 41.7, 40.4, 35.3, 27.8, 19.6, 13.3, 10.7},
        {21.3, 25.7, 31.5, 36.8, 40.0, 41.1, 40.4, 37.8, 33.2, 27.4, 22.2, 19.9},
        {29.6, 32.6, 35.9, 38.0, 38.5, 38.4, 38.3, 38.0, 36.4, 33.4, 30.1, 28.5},
        {36.2, 37.4, 37.8, 36.7, 34.8, 33.5, 34.0, 35.7, 37.2, 37.3, 36.3, 35.7},
        {39.5, 39.3, 37.7, 34.5, 31.1, 29.2, 29.9, 32.9, 36.3, 38.5, 39.3, 39.4},
        {42.8, 37.1, 28.6, 19.6, 12.9, 10.0, 11.3, 16.6, 24.9, 34.0, 41.2, 44.5},
        {41.0, 32.4, 21.2, 10.9, 4.5, 2.2, 3.1, 8.0, 17.0, 28.4, 38.7, 43.7},
        {43.3, 27.8, NAN, 0.0, 0.0, 0.0, 0.0, 0.0, NAN, NAN, 39.4, 47.8}
      };

    for (k1 = 0; k1 < 9; k1++) {
      lat = lats_Table1_10_1[k1] * deg_to_rad;

      for (k2 = 0; k2 < 12; k2++) {
        if (std::isfinite(H_oh_Table1_10_1[k1][k2])) {
          doy = doys_Table1_6_1[k2];

          sun_hourangles(doy, lat, 0., 0., int_cos_theta, int_sin_beta);
          solar_radiation_extraterrestrial(doy, int_cos_theta, H_o);

          if (ZRO(H_oh_Table1_10_1[k1][k2])) {
            // Check for small absolute difference
            EXPECT_NEAR(H_o[0], H_oh_Table1_10_1[k1][k2], tol6)
              << "Duffie & Beckman 2013: Table 1.10.1:"
              << " latitude = " << lats_Table1_10_1[k1]
              << ", month = " << k2 + 1
              << " int(cos(theta)) = " << int_cos_theta[0] << "\n";

          } else {
            // Check for small relative difference (< 10%)
            res_ratio = H_o[0] / H_oh_Table1_10_1[k1][k2];

            EXPECT_NEAR(res_ratio, 1., tol1)
              << "Duffie & Beckman 2013: Table 1.10.1:"
              << " latitude = " << lats_Table1_10_1[k1]
              << ", month = " << k2 + 1
              << " int(cos(theta)) = " << int_cos_theta[0] << "\n";
          }
        }
      }
    }


      // Sunset hour angle: every day has six hours on equator
      ahou = sunset_hourangle(rlat_equator, declin);
      EXPECT_NEAR(ahou, six_hours, tol6) << "doy = " << i;
    }
    // Duffie & Beckman 2013: Example 1.10.1
    doy = 105;
    sun_hourangles(doy, lat_Madison_WI, 0., 0., int_cos_theta, int_sin_beta);
    solar_radiation_extraterrestrial(doy, int_cos_theta, H_o);
    EXPECT_NEAR(H_o[0], 33.8, 2. * tol1)
      << "Duffie & Beckman 2013: Example 1.10.1\n";

    // Duffie & Beckman 2013: Example 2.11.1
    doy = 246;
    sun_hourangles(doy, lat_StLouis_MO, 0., 0., int_cos_theta, int_sin_beta);
    solar_radiation_extraterrestrial(doy, int_cos_theta, H_o);
    EXPECT_NEAR(H_o[0], 33.0, 7. * tol1)
      << "Duffie & Beckman 2013: Example 2.11.1\n";

    // Duffie & Beckman 2013: Example 2.12.1
    doy = 162;
    sun_hourangles(doy, lat_Madison_WI, 0., 0., int_cos_theta, int_sin_beta);
    solar_radiation_extraterrestrial(doy, int_cos_theta, H_o);
    EXPECT_NEAR(H_o[0], 41.8, tol1)
      << "Duffie & Beckman 2013: Example 2.12.1\n";
  }


    // Loop through latitudes
    for (i = 0; i <= 10; i++) {
      // Sunset hour angle: every location has six hours on
      // equinoxes (when declin = 0)
      rlat = (-90. + 180. * (i - 0.) / 10.) * deg_to_rad;
      ahou = sunset_hourangle(rlat, 0.);
      EXPECT_NEAR(ahou, six_hours, tol3) << "lat = " << rlat;
    }
  }




  // Test saturated vapor pressure function
  TEST(SW2_PET_Test, svapor)
  {
    //Declare INPUTS
    double temp[] = {30,35,40,45,50,55,60,65,70,75,20,-35,-12.667,-1,0}; // These are test temperatures, in degrees Celcius.
    double expOut[] = {32.171, 43.007, 56.963, 74.783, 97.353, 125.721, 161.113,
                       204.958, 258.912, 324.881, 17.475, 0.243, 1.716, 4.191,
                       4.509}; // These are the expected outputs for the svapor function.

    //Declare OUTPUTS
    double vapor;

    //Begin TEST
    for (int i = 0; i <15; i++) {
      vapor = svapor(temp[i]);

      EXPECT_NEAR(expOut[i], vapor, tol3);
    }
  }


  //Test petfunc by manipulating each input individually.
  TEST(SW2_PET_Test, petfunc)
  {
    //Begin TEST for avgtemp input variable
    //Declare INPUTS
    unsigned int doy = 2; //For the second day in the month of January
    double rlat = 0.681, elev = 1000, slope = 0, aspect = -1, reflec = 0.15,
      humid = 61,windsp = 1.3, cloudcov = 71, transcoeff = 1;
    double temp, check;
    double avgtemps[] = {30,35,40,45,50,55,60,65,70,75,20,-35,-12.667,-1,0}; // These are test temperatures, in degrees Celcius.

    //Declare INPUTS for expected returns
    double expReturnTemp[] = {0.201, 0.245, 0.299, 0.364, 0.443, 0.539, 0.653,
                              0.788, 0.948, 1.137, 0.136, 0.01, 0.032, 0.057,
                              0.060}; // These are the expected outcomes for the variable arads.

    for (int i = 0; i <15; i++)
    {
      temp = avgtemps[i]; //Uses array of temperatures for testing for input into temp variable.
      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid,
                      windsp, cloudcov, transcoeff);

      EXPECT_NEAR(check, expReturnTemp[i], tol3); //Tests the return of the petfunc against the expected return for the petfunc.
    }

    //Begin TEST for rlat input variable.  Inputs outside the range of [-1.169,1.169] produce the same output, 0.042.  Rlat values outside this range represent values near the poles.
    //INPUTS
    temp = 25, cloudcov = 71, humid = 61, windsp = 1.3;
    double rlats[] = {-1.570796, -0.7853982, 0, 0.7853982, 1.570796}; //Initialize array of potential latitudes, in radians, for the site.

   //Declare INPUTS for expected returns
    double expReturnLats[] = {0.042, 0.420, 0.346, 0.134, 0.042}; //These are the expected returns for petfunc while manipulating the rlats input variable.

    for (int i = 0; i < 5; i++)
    {
      rlat = rlats[i]; //Uses array of latitudes initialized above.
      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid,
                      windsp, cloudcov, transcoeff);

      EXPECT_NEAR(check, expReturnLats[i], tol3); //Tests the return of the petfunc against the expected return for the petfunc.
    }
    //Begin TEST for elev input variable, testing from -413 meters (Death Valley) to 8727 meters (~Everest).
    //INPUTS
    rlat = 0.681;
    double elevT[] = {-413,0,1000,4418,8727};

    //Declare INPUTS for expected returns
    double expReturnElev[] = {0.181,0.176,0.165,0.130,0.096};

    for (int i = 0; i < 5; i++)
    {
      check = petfunc(doy, temp, rlat, elevT[i], slope, aspect, reflec, humid,
                      windsp, cloudcov, transcoeff);

      EXPECT_NEAR(check, expReturnElev[i], tol3); //Tests the return of the petfunc against the expected return for the petfunc.
    }
    //Begin TEST for slope input variable
    //INPUTS
    elev = 1000;
    aspect = 180; // south-facing slope
    double slopeT[] = {0,15,34,57,90};

    //Declare INPUTS for expected returns
    double expReturnSlope[] = {0.1650, 0.2398, 0.3148, 0.3653, 0.3479};

    for (int i = 0; i < 5; i++)
    {
      check = petfunc(doy, temp, rlat, elev, slopeT[i], aspect, reflec, humid,
                      windsp, cloudcov, transcoeff);

      EXPECT_NEAR(check, expReturnSlope[i], tol3); //Tests the return of the petfunc against the expected return for the petfunc.
    }

    //Begin TEST for aspect input variable
    //INPUTS
    slope = 5;
    double aspectT[] = {0, 46, 112, 253, 358};

    //Declare INPUTS for expected returns
    double expReturnAspect[] = {0.138, 0.146, 0.175, 0.172, 0.138};

    for (int i = 0; i < 5; i++) {
      check = petfunc(doy, temp, rlat, elev, slope, aspectT[i], reflec, humid,
                      windsp, cloudcov, transcoeff);

      EXPECT_NEAR(check, expReturnAspect[i], tol3);
    }

    //Begin TEST for reflec input variable
    //INPUTS
    aspect = -1, slope = 0;
    double reflecT[] = {0.1, 0.22, 0.46, 0.55, 0.98};

    //Declare INPUTS for expected returnsdouble expReturnSwpAvg = 0.00000148917;
    double expReturnReflec[] = {0.172, 0.155, 0.120, 0.107, 0.045};

    for (int i = 0; i < 5; i++)
    {
      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflecT[i], humid,
                      windsp, cloudcov, transcoeff);

      EXPECT_NEAR(check, expReturnReflec[i], tol3);
    }

    //Begin TEST for humidity input varibable.
    //INPUTS
    reflec = 0.15;
    double humidT[] = {2, 34, 56, 79, 89};

    //Declare INPUTS for expected returns.
    double expReturnHumid[] = {0.242, 0.218, 0.176, 0.125, 0.102};

    for (int i = 0; i < 5; i++)
    {
      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humidT[i],
                      windsp, cloudcov, transcoeff);

      EXPECT_NEAR(check, expReturnHumid[i], tol3);
    }

    //Begin TEST for windsp input variable.
    //INPUTS
    humid = 61, windsp = 0;

    //Declare INPUTS for expected returns.
    double expReturnWindsp[] = {0.112, 0.204, 0.297, 0.390, 0.483, 0.576, 0.669,
                                0.762, 0.855, 0.948, 1.041, 1.134, 1.227, 1.320,
                                1.413, 1.506, 1.599, 1.692, 1.785, 1.878, 1.971,
                                2.064, 2.157};

    for (int i = 0; i < 23; i++)
    {
      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid,
                      windsp, cloudcov, transcoeff);

      EXPECT_NEAR(check, expReturnWindsp[i], tol3);

      windsp += 2.26; //Increments windsp input variable.
    }

    //Begin TEST for cloudcov input variable.
    //INPUTS
    windsp = 1.3;
    double cloudcovT[] = {1, 12, 36, 76, 99};

    //Declare INPUTS for expected returns.
    double expReturnCloudcov[] = {0.148, 0.151, 0.157, 0.166, 0.172};

    for (int i = 0; i < 5; i++)
    {
      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid,
                      windsp, cloudcovT[i], transcoeff);

      EXPECT_NEAR(check, expReturnCloudcov[i], tol3);

      cloudcov += 4.27; //Incrments cloudcov input variable.
    }
    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }

}
