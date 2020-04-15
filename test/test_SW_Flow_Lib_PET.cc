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

#include "../SW_Flow_lib.h"

#include "sw_testhelpers.h"


namespace
{

  //Test svapor function by manipulating variable temp.
  TEST(SWFlowTestPET, svapor)
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

      EXPECT_NEAR(expOut[i], vapor, tol3); // Testing input array temp[], expected output is expOut[].
    }

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
  }


  // Test solar declination
  TEST(SWFlowTestPET, solar_declination)
  {
    double declin,
      declin_max = 0.4094, // should rather be 0.409 = (23+26/60)*pi/180
      declin_min = -declin_max;
    int i,
      doy_Mar_equinox = 80,
      doy_Sep_equinox = 266,
      doy_Jun_solstice = 173,
      doy_Dec_solstice = 355;

    // Loop through each day of year
    for (i = 1; i <= 365; i++) {
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
    }

  }


  //Test petfunc by manipulating each input individually.
  TEST(SWFlowTestPET, petfunc)
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

    //Begin TEST for cloudcov input variable.
    //INPUTS
    cloudcov = 71;
    double transcoeffT[] = {0.01, 0.11, 0.53, 0.67, 0.89};

    //Declare INPUTS for expected returns.
    double expReturnTranscoeff = 0.1650042;
    //The same value is returned for every tested transcoeff input.

    for (int i = 0; i < 5; i++)
    {
      check = petfunc(doy, temp, rlat, elev, slope, aspect, reflec, humid,
                      windsp, cloudcov, transcoeffT[i]);

      EXPECT_NEAR(check, expReturnTranscoeff, tol3);
    }

    //Reset to previous global states.
    Reset_SOILWAT2_after_UnitTest();
    }

}
