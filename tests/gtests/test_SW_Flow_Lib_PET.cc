#include "gmock/gmock.h"
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <float.h>
#include <cmath>
#include <math.h>
#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>
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
#include "external/pcg/pcg_basic.h"
#include "include/SW_Main_lib.h"

#include "include/SW_Flow_lib_PET.h"

#include "tests/gtests/sw_testhelpers.h"

using ::testing::StrEq;

namespace
{
  // Test solar position
  TEST(AtmDemandTest, SolarPosSolarPosition)
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


  // Test sun hour angles for horizontal and tilted surfaces
  TEST(AtmDemandTest, SolarPosSW_HourAnglesSymmetries)
  {
    //------ Check expectations on some symmetries
    //  - Expectation 1: Horizontal sunset/sunrise:
    //      symmetric in time reflected around (solar) noon
    //  - Expectation 2: Daylength:
    //      symmetric in aspect reflected around South aspect
    //  - Expectation 3: Tilted sunset(s)/sunrises(s):
    //      symmetric in aspect reflected around South aspect
    //  - Expectation 4: Daylength:
    //      approximately symmetric in day of year reflected around
    //      June solstice, 172 ± ddoy;
    //  - Expectation 5: Daylength:
    //      approximately symmetric in day of year shifted by half-year,
    //      latitude reflected on equator, and aspect flipped by 180-degree

    // Symmetry is approximate for expectations 4-5
    // because slightly asymmetric nature of declination in regard to solstices
    // can cause large differences in calculated sun hour angles for
    // some combinations of DOY, latitude, slope, and aspect
    // (particularly near equinoxes and near "edges" of shading):
    //    --> not unit tested here, but see
    //        `SW2_SolarPosition_Test__hourangles_by_lat_and_doy`

    SW_ATMD SW_AtmDemand;

    LOG_INFO LogInfo;
    sw_init_logs(NULL, &LogInfo); // Initialize logs and silence warn/error reporting

    int
      k, k2, ilat, itime, isl, iasp,
      doys[14] =
        {1, 17, 47, 75, 105, 135, 162, 198, 228, 258, 288, 318, 344, 366},
      doy_used[4][14],
      doy_Jun_solstice = 172;
    double
      rad_to_hours = 12. / swPI,
      latitude,
      latitude_used[4][14],
      slope,
      aspect,
      aspect_used[4][14],
      o[4][14][7],
      int_cos_theta[2],
      int_sin_beta[2],
      daylength[4][14];
    std::ostringstream msg, msg2;


    for (isl = 0; isl <= 8; isl++) {
      slope = 90. * isl / 8.;
      slope *= deg_to_rad;

      for (iasp = 0; iasp <= 8; iasp++) {
        aspect = (isl == 0) ? 0. : 180. * iasp / 8.;
        aspect *= deg_to_rad;

        for (ilat = 0; ilat <= 8; ilat++) {
          latitude = 180. * (ilat - 4.) / 8.;
          latitude *= deg_to_rad;

          for (k = 0; k < 2; k++) {
            for (itime = 0; itime < 14; itime++) {

              switch (k) {
                case 0: // Reference case
                  doy_used[k][itime] = doys[itime];
                  aspect_used[k][itime] = aspect;
                  latitude_used[k][itime] = latitude;
                  break;

                case 1: // Symmetry in aspect
                  doy_used[k][itime] = doys[itime];
                  aspect_used[k][itime] = -aspect;
                  latitude_used[k][itime] = latitude;
                  break;

                case 2: // (Approximate) Symmetry in DOY
                  doy_used[k][itime] =
                    (2 * doy_Jun_solstice - doys[itime]) % 365;
                  aspect_used[k][itime] = aspect;
                  latitude_used[k][itime] = latitude;
                  break;

                case 3: // (Approximate) Symmetry in DOY, latitude, and aspect
                  doy_used[k][itime] = (doys[itime] + 183) % 365;
                  if (aspect >= 0.) {
                    aspect_used[k][itime] = aspect - swPI;
                  } else {
                    aspect_used[k][itime] = aspect + swPI;
                  }
                  latitude_used[k][itime] = -latitude;
                  break;

                default:
                  LogError(&LogInfo, LOGERROR,
                           "Error in SW2_SolarPosition_Test__hourangles_symmetries");
              }

              sw_fail_on_error(&LogInfo); // exit test program if unexpected error

              SW_PET_init_run(&SW_AtmDemand); // Init radiation memoization

              // Calculate sun hour angles
              sun_hourangles(
                &SW_AtmDemand,
                doy_used[k][itime],
                latitude_used[k][itime],
                slope,
                aspect_used[k][itime],
                o[k][itime],
                int_cos_theta,
                int_sin_beta
              );

              //------ Expectation 1: horizontal sunset/sunrise:
              // symmetric in time reflected around (solar) noon
              EXPECT_NEAR(o[k][itime][6], -o[k][itime][1], tol9);

              // Calculate number of daylight hours
              if (isl == 0) {
                daylength[k][itime] = o[k][itime][6] - o[k][itime][1];
              } else {
                daylength[k][itime] =
                    o[k][itime][3] - o[k][itime][2] \
                  + o[k][itime][5] - o[k][itime][4];
              }

              daylength[k][itime] *= rad_to_hours;
            }
          }


          for (itime = 0; itime < 14; itime++) {
            msg.str("");
            msg <<
              "doy = " << doy_used[1][itime] <<
              ", lat = " <<
                round(latitude_used[1][itime] * rad_to_deg * 100.) / 100. <<
              ", slope = " <<
                round(slope * rad_to_deg * 100.) / 100. <<
              ", aspect = " <<
                round(aspect_used[0][itime] * rad_to_deg * 100.) / 100. <<
              "|" <<
                round(aspect_used[1][itime] * rad_to_deg * 100.) / 100.;

            msg2.str("");
            for (k2 = 0; k2 < 7; k2++) {
              msg2 <<
                "o[0|1][" << k2 << "] = " <<
                o[0][itime][k2] << "|" << o[1][itime][k2];

              if (k2 < 6) msg2 << ", ";
            }


            //------ Expectation 2: Daylength:
            // symmetric in aspect reflected around South aspect: 0±abs(asp)
            EXPECT_NEAR(daylength[0][itime], daylength[1][itime], tol9) <<
              "symmetry (reflected aspect) of daylength for " <<
              msg.str();

            //------ Expectation 3: Tilted sunrise/sunset:
            // negatively symmetric in aspect reflected around South aspect
            for (k2 = 0; k2 < 4; k2++) {
              // k2 = 0: `o[.][2]` (first sunrise) vs `o[.][5]` (final sunset)
              // k2 = 1: `o[.][3]` (first sunset) vs `o[.][4]` (second sunrise)

              if (
                missing(o[0][itime][2 + k2]) || missing(o[1][itime][5 - k2])
              ) {
                // if one of (first sunset, second sunrise) is missing,
                // then both should be missing
                EXPECT_TRUE(
                  missing(o[0][itime][2 + k2]) && missing(o[1][itime][5 - k2])
                ) <<
                  "symmetry (reflected aspect) of tilted sunrise/sunset for " <<
                  msg.str() <<
                  " k2 = " << k2 << " (missing values);" <<
                  " hour angles: " << msg2.str();

              } else {
                // no values missing
                EXPECT_NEAR(
                  o[0][itime][2 + k2],
                  -o[1][itime][5 - k2],
                  tol9
                ) <<
                  "symmetry (reflected aspect) of tilted sunrise/sunset for " <<
                  msg.str() <<
                  " k2 = " << k2 <<
                  "; hour angles: " << msg2.str();
              }
            }
          }
        }

        if (isl == 0) {
          break;
        }
      }
    }
  }


  #ifdef SW2_SolarPosition_Test__hourangles_by_lat_and_doy
  // Run SOILWAT2 unit tests with flag
  // ```
  //   CPPFLAGS=-DSW2_SolarPosition_Test__hourangles_by_lat_and_doy make test test_run
  // ```
  //
  // Produce plots based on output generated above
  // ```
  //   Rscript tools/plot__SW2_SolarPosition_Test__hourangles_by_lat_and_doy.R
  // ```
  TEST(AtmDemandTest, SolarPosHourAnglesByLatAndDoy)
  {
    int k, ilat, idoy, isl, iasp, length_strnum;
    double
      rad_to_hours = 12. / swPI,
      slope = 0.,
      aspect = 0.,
      aspects[9] = {-180., -120., -90., -60., 0., 60., 90., 120., 180.},
      sun_angles[7],
      int_cos_theta[2],
      int_sin_beta[2],
      daylength_H,
      daylength_T;

    FILE *fp;
    char
      *strnum,
      fname[FILENAME_MAX];


    for (isl = 0; isl <= 3; isl++) {
      slope = 90. * isl / 3.;

      for (iasp = 0; iasp < 9; iasp++) {
        aspect = (isl == 0) ? 0. : aspects[iasp];

/*
    for (isl = 0; isl <= 8; isl++) {
      slope = 90. * isl / 8.;

      for (iasp = 0; iasp <= 16; iasp++) {
        aspect = (isl == 0) ? 0. : 180. * (iasp - 8.) / 8.;
*/

        // Output file
        strcpy(fname, output_prefix);
        strcat(
          fname,
          "Table__SW2_SolarPosition_Test__hourangles_by_lat_and_doy"
        );

        strcat(fname, "__slope");
        length_strnum = snprintf(NULL, 0, "%d", (int) slope);
        strnum = (char *) malloc(length_strnum + 1);
        snprintf(strnum, length_strnum + 1, "%d", (int) slope);
        strcat(fname, strnum);
        free(strnum);

        strcat(fname, "__aspect");
        length_strnum = snprintf(NULL, 0, "%d", (int) aspect);
        strnum = (char *) malloc(length_strnum + 1);
        snprintf(strnum, length_strnum + 1, "%d", (int) aspect);
        strcat(fname, strnum);
        free(strnum);

        strcat(fname, ".csv");

        fp = OpenFile(fname, "w");

        // Column names
        fprintf(
          fp,
          "DOY, Latitude, Slope, Aspect, Declination"
          ", omega_indicator, "
          "oH_sunrise, oT1_sunrise, oT1_sunset, "
          "oT2_sunrise, oT2_sunset, oH_sunset"
          ", Daylight_horizontal_hours, Daylight_tilted_hours"
          "\n"
        );


        // Loop over each DOY and 1-degree latitude bands
        for (ilat = -90; ilat <= 90; ilat++) {
          for (idoy = 1; idoy <= 366; idoy++) {
            fprintf(
              fp,
              "%d, %d, %.2f, %.2f, %f",
              idoy,
              ilat,
              slope,
              aspect,
              solar_declination(idoy) * rad_to_deg
            );

            sun_hourangles(
              idoy,
              ilat * deg_to_rad,
              slope * deg_to_rad,
              aspect * deg_to_rad,
              sun_angles,
              int_cos_theta,
              int_sin_beta
            );

            for (k = 0; k < 7; k++) {
              fprintf(fp, ", %f", sun_angles[k]);
            }

            // Calculate numbers of daylight hours
            daylength_H = sun_angles[6] - sun_angles[1];

            if (isl == 0 || missing(aspect)) {
              daylength_T = daylength_H;

            } else {
              daylength_T =
                sun_angles[3] - sun_angles[2] +
                sun_angles[5] - sun_angles[4];
            }

            fprintf(
              fp,
              ", %f, %f\n",
              daylength_H * rad_to_hours,
              daylength_T * rad_to_hours
            );

            fflush(fp);
          }

          SW_PET_init_run(); // Re-init radiation memoization (for new latitude)
        }


        // Clean up
        CloseFile(&fp);

        if (isl == 0) {
          break;
        }
      }
    }
  }
  #endif // end of SW2_SolarPosition_Test__hourangles_by_lat_and_doy



  #ifdef SW2_SolarPosition_Test__hourangles_by_lats
  // Run SOILWAT2 unit tests with flag
  // ```
  //   CPPFLAGS=-DSW2_SolarPosition_Test__hourangles_by_lats make test test_run
  // ```
  //
  // Produce plots based on output generated above
  // ```
  //   Rscript tools/plot__SW2_SolarPosition_Test__hourangles_by_lats.R
  // ```
  TEST(AtmDemandTest, SolarPosHourAnglesByLats)
  {
    int k, ilat, idoy, isl, iasp, iasp2,
      // day of nonleap year Mar 18 (one day before equinox), Jun 21 (solstice),
      // Sep 24 (one day before equinox), and Dep 21 (solstice)
      doys[4] = {79, 172, 263, 355};
    double
      rlat,
      rslope,
      raspect,
      dangle2[5] = {-10., -1., 0., 1., 10.},
      sun_angles[7],
      int_cos_theta[2],
      int_sin_beta[2];

    FILE *fp;
    char fname[FILENAME_MAX];

    strcpy(fname, output_prefix);
    strcat(fname, "Table__SW2_SolarPosition_Test__hourangles_by_lats.csv");
    fp = OpenFile(fname, "w");

    // Column names
    fprintf(
      fp,
      "DOY, Latitude, Slope, Aspect, Declination"
      ", omega_indicator, "
      "oH_sunrise, oT1_sunrise, oT1_sunset, oT2_sunrise, oT2_sunset, oH_sunset"
      ", int_cos_theta0, int_cos_thetaT, int_sin_beta0, int_sin_betaT"
      "\n"
    );


    for (ilat = -90; ilat <= 90; ilat++) {
      rlat = ilat * deg_to_rad;

      for (isl = 0; isl <= 8; isl++) {
        rslope = 90. * isl / 8. * deg_to_rad;

        for (iasp = 0; iasp < 9; iasp++) {
          for (iasp2 = 0; iasp2 < 5; iasp2++) {
            raspect = (iasp - 4.) / 4. * swPI + dangle2[iasp2] * deg_to_rad;

            for (idoy = 0; idoy < 4; idoy++) {

              fprintf(
                fp,
                "%d, %.2f, %.2f, %.2f, %f",
                doys[idoy],
                rlat * rad_to_deg,
                rslope * rad_to_deg,
                raspect * rad_to_deg,
                solar_declination(doys[idoy])
              );

              sun_hourangles(
                doys[idoy], rlat, rslope, raspect,
                sun_angles,
                int_cos_theta,
                int_sin_beta
              );

              for (k = 0; k < 7; k++) {
                fprintf(fp, ", %f", sun_angles[k]);
              }

              fprintf(
                fp,
                ", %f, %f, %f, %f\n",
                int_cos_theta[0], int_cos_theta[1],
                int_sin_beta[0], int_sin_beta[1]
              );

              fflush(fp);
            }

            SW_PET_init_run(); // Re-init radiation memoization
          }
        }
      }
    }

    CloseFile(&fp);
  }
  #endif // end of SW2_SolarPosition_Test__hourangles_by_lats


  // Test extraterrestrial solar radiation
  //   Comparison against examples by Duffie & Beckman 2013 are expected to
  //   deviate in value, but show similar patterns, because equations for
  //   (i) sun-earth distance equation and (ii) solar declination differ
  TEST(AtmDemandTest, SolarRadiationExtraterrestrial)
  {
    SW_ATMD SW_AtmDemand;

    unsigned int k1, k2, doy;
    double
      lat,
      lat_Madison_WI = 43. * deg_to_rad, // Duffie & Beckman 2013: Ex 1.6.1
      lat_StLouis_MO = 38.6 * deg_to_rad, // Duffie & Beckman 2013: Ex 2.11.1
      sun_angles[7], int_cos_theta[2], int_sin_beta[2],
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

        SW_PET_init_run(&SW_AtmDemand); // Init radiation memoization (for new location)

        if (std::isfinite(H_oh_Table1_10_1[k1][k2])) {
          doy = doys_Table1_6_1[k2];

          sun_hourangles(
            &SW_AtmDemand,
            doy, lat, 0., 0.,
            sun_angles, int_cos_theta, int_sin_beta
          );
          solar_radiation_extraterrestrial(SW_AtmDemand.memoized_G_o, doy,
                                           int_cos_theta, H_o);

          if (ZRO(H_oh_Table1_10_1[k1][k2])) {
            // Check for small absolute difference
            EXPECT_NEAR(H_o[0], H_oh_Table1_10_1[k1][k2], tol6)
              << "Duffie & Beckman 2013: Table 1.10.1:"
              << " latitude = " << lats_Table1_10_1[k1]
              << ", month = " << k2 + 1
              << ", int(cos(theta)) = " << int_cos_theta[0] << "\n";

          } else {
            // Check for small relative difference (< 10%)
            res_ratio = H_o[0] / H_oh_Table1_10_1[k1][k2];

            EXPECT_NEAR(res_ratio, 1., tol1)
              << "Duffie & Beckman 2013: Table 1.10.1:"
              << " latitude = " << lats_Table1_10_1[k1]
              << ", month = " << k2 + 1
              << ", int(cos(theta)) = " << int_cos_theta[0] << "\n";
          }
        }
      }
    }


    // Duffie & Beckman 2013: Example 1.10.1
    SW_PET_init_run(&SW_AtmDemand); // Re-init radiation memoization
    doy = 105;
    sun_hourangles(
      &SW_AtmDemand,
      doy, lat_Madison_WI, 0., 0.,
      sun_angles, int_cos_theta, int_sin_beta
    );
    solar_radiation_extraterrestrial(SW_AtmDemand.memoized_G_o, doy,
                                     int_cos_theta, H_o);
    EXPECT_NEAR(H_o[0], 33.8, 2. * tol1)
      << "Duffie & Beckman 2013: Example 1.10.1\n";


    // Duffie & Beckman 2013: Example 2.11.1
    SW_PET_init_run(&SW_AtmDemand); // Re-init radiation memoization
    doy = 246;
    sun_hourangles(
      &SW_AtmDemand,
      doy, lat_StLouis_MO, 0., 0.,
      sun_angles, int_cos_theta, int_sin_beta
    );
    solar_radiation_extraterrestrial(SW_AtmDemand.memoized_G_o, doy, int_cos_theta, H_o);
    EXPECT_NEAR(H_o[0], 33.0, 7. * tol1)
      << "Duffie & Beckman 2013: Example 2.11.1\n";


    // Duffie & Beckman 2013: Example 2.12.1
    SW_PET_init_run(&SW_AtmDemand); // Re-init radiation memoization
    doy = 162;
    sun_hourangles(
      &SW_AtmDemand,
      doy, lat_Madison_WI, 0., 0.,
      sun_angles, int_cos_theta, int_sin_beta
    );
    solar_radiation_extraterrestrial(SW_AtmDemand.memoized_G_o, doy, int_cos_theta, H_o);
    EXPECT_NEAR(H_o[0], 41.8, tol1)
      << "Duffie & Beckman 2013: Example 2.12.1\n";
  }


  // Test solar radiation: global horizontal and tilted
  //   Comparison against examples by Duffie & Beckman 2013 are expected to
  //   deviate in value, but show similar patterns, because
  //   (i) calculations for H_oh differ
  //       (see `SW2_SolarRadiationiation_Test.extraterrestrial`),
  //   (ii) we calculate H_gh while they use measured H_gh values, and
  //   (iii) separation models differ, etc.
  TEST(AtmDemandTest, SolarRadiationGlobal)
  {
    SW_ATMD SW_AtmDemand;
    SW_PET_init_run(&SW_AtmDemand); // Init radiation memoization

    LOG_INFO LogInfo;
    sw_init_logs(NULL, &LogInfo); // Initialize logs and silence warn/error reporting

    unsigned int k;

    // Duffie & Beckman 2013: Table 1.6.1
    unsigned int doys_Table1_6_1[12] = {
      17, 47, 75, 105, 135, 162, 198, 228, 258, 288, 318, 344
    };
    unsigned int desc_rsds = 0; // `rsds` represents daily irradiation [MJ / m2]

    double
      H_gt, H_ot, H_oh, H_gh,
      rsds,
      cc, actual_vap_pressure,

      // Duffie & Beckman 2013: Example 2.19.1
      H_Ex2_19_1[3][12] = {
        // H_oh [MJ / m2]
        {13.37, 18.81, 26.03, 33.78, 39.42, 41.78, 40.56, 35.92, 28.80, 20.90, 14.62, 11.91},
        // H_gh [MJ / m2]
        {6.44, 9.89, 12.86, 16.05, 21.36, 23.04, 22.58, 20.33, 14.59, 10.48, 6.37, 5.74},
        // H_gt [MJ / m2]
        {13.7, 17.2, 15.8, 14.7, 16.6, 16.5, 16.8, 17.5, 15.6, 15.2, 11.4, 12.7}
      },
      albedo[12] =
        {0.7, 0.7, 0.4, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.4},

      // Climate normals for Madison, WI
      // "WMO Climate Normals for MADISON/DANE CO REGIONAL ARPT, WI 1961–1990".
      // National Oceanic and Atmospheric Administration. Retrieved Jul 3, 2020.
      // ftp://ftp.atdd.noaa.gov/pub/GCOS/WMO-Normals/TABLES/REG_IV/US/GROUP4/72641.TXT
      cloud_cover1[12] =
        // Element 20:  Sky Cover (Cloud Cover)
        // {66.25, 66.25, 70, 67.5, 65, 60, 57.5, 57.5, 60, 63.75, 72.5, 71.25},
        // replaced observed with estimated values to match `H_Ex2_19_1`:
        // replaced ~ -61 + 1.661 * observed
        {53., 47.5, 54., 53., 40., 35., 35., 30., 46., 50., 63., 52.},
      cloud_cover2[12] =
        // derived from observed `rsds` (`H_Ex2_19_1["H_gh"][]`) and calculated `H_gh`
        // note: this should be identical to `cloud_cover1[]`
        {39.9, 37.7, 45.6, 49.0, 36.2, 32.9, 30.6, 28.7, 40.6, 41.8, 50.7, 37.6},
        // Element 11:  Relative Humidity (%), MN3HRLY (Statistic 94):  Mean of 3-Hourly Observations
      rel_humidity[12] =
        {74.5, 73.1, 71.4, 66.3, 65.8, 68.3, 71.0, 74.4, 76.8, 73.2, 76.9, 78.5},
        // Element 01:  Dry Bulb Temperature (deg C)
      air_temp_mean[12] =
        {-8.9, -6.3, 0.2, 7.4, 13.6, 19, 21.7, 20.2, 15.4, 9.4, 1.9, -5.7};


    // Duffie & Beckman 2013: Example 2.19.1
    for (k = 0; k < 12; k++) {

      actual_vap_pressure = actualVaporPressure1(rel_humidity[k], air_temp_mean[k]);

      //--- Test without observed radiation: missing `rsds`; `H_gh` calculated
      cc = cloud_cover1[k];
      rsds = SW_MISSING;

      H_gt = solar_radiation(
        &SW_AtmDemand,
        doys_Table1_6_1[k],
        43. * deg_to_rad, // latitude
        226., // elevation
        60 * deg_to_rad, // slope
        0., // aspect
        albedo[k],
        &cc,
        actual_vap_pressure,
        rsds,
        desc_rsds,
        &H_oh,
        &H_ot,
        &H_gh,
        &LogInfo
      );
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      EXPECT_NEAR(H_oh, H_Ex2_19_1[0][k], tol0)
        << "Duffie & Beckman 2013: Example 2.19.1 (missing rsds), H_oh: "
        << "month = " << k + 1 << "\n";
      // Feb/March deviate by ±1.25; other months by less than ±1
      EXPECT_NEAR(H_gh, H_Ex2_19_1[1][k], 1.25 * tol0)
        << "Duffie & Beckman 2013: Example 2.19.1 (missing rsds), H_gh: "
        << "month = " << k + 1 << "\n";
      EXPECT_NEAR(H_gt, H_Ex2_19_1[2][k], 1.25 * tol0)
        << "Duffie & Beckman 2013: Example 2.19.1 (missing rsds), H_gt: "
        << "month = " << k + 1 << "\n";


      //--- Test with previously calculated `H_gh` and missing cloud cover
      cc = SW_MISSING;
      rsds = H_gh; // calculated using `cloud_cover1[]`

      H_gt = solar_radiation(
        &SW_AtmDemand,
        doys_Table1_6_1[k],
        43. * deg_to_rad, // latitude
        226., // elevation
        60 * deg_to_rad, // slope
        0., // aspect
        albedo[k],
        &cc,
        actual_vap_pressure,
        rsds,
        desc_rsds,
        &H_oh,
        &H_ot,
        &H_gh,
        &LogInfo
      );
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      // Expect: observed `rsds` (for `desc_rsds = 0`) is equal to `H_gh`
      EXPECT_DOUBLE_EQ(rsds, H_gh);
      // Expect: calculated cloud cover is equal to cloud cover previously
      // used to determine "observed" `rsds`
      EXPECT_DOUBLE_EQ(cc, cloud_cover1[k]);


      //--- Test with observed radiation `rsds` and missing cloud cover
      cc = SW_MISSING;
      rsds = H_Ex2_19_1[1][k];

      H_gt = solar_radiation(
        &SW_AtmDemand,
        doys_Table1_6_1[k],
        43. * deg_to_rad, // latitude
        226., // elevation
        60 * deg_to_rad, // slope
        0., // aspect
        albedo[k],
        &cc,
        actual_vap_pressure,
        rsds,
        desc_rsds,
        &H_oh,
        &H_ot,
        &H_gh,
        &LogInfo
      );
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      EXPECT_NEAR(H_oh, H_Ex2_19_1[0][k], tol0)
        << "Duffie & Beckman 2013: Example 2.19.1 (observed rsds), H_oh: "
        << "month = " << k + 1 << "\n";
      EXPECT_NEAR(H_gh, H_Ex2_19_1[1][k], tol0)
        << "Duffie & Beckman 2013: Example 2.19.1 (observed rsds), H_gh: "
        << "month = " << k + 1 << "\n";
      // Nov deviates by -2.8; Oct-Jan by ±1.4; other months by less than ±1
      EXPECT_NEAR(H_gt, H_Ex2_19_1[2][k], 3 * tol0)
        << "Duffie & Beckman 2013: Example 2.19.1 (observed rsds), H_gt: "
        << "month = " << k + 1 << "\n";

      // Cloud cover estimated from observed `rsds` and calculated `H_gh`
      EXPECT_NEAR(cc, cloud_cover2[k], tol1)
        << "Duffie & Beckman 2013: Example 2.19.1 (observed rsds), cloud cover: "
        << "month = " << k + 1 << "\n";
    }
  }




  // Test saturation vapor pressure functions
  TEST(AtmDemandTest, PETsvp)
  {
    int i;
    double
      // Temperature [C]
      temp_C[] = {-30, -20, -10, 0, 10, 20, 30, 40, 50, 60},

      // Expected saturation vapor pressure [kPa]
      check_svp,
      expected_svp[] = {
        0.0380009, 0.103226, 0.2598657, 0.6112912,
        1.2281879, 2.3393207, 4.247004, 7.3849328, 12.3517837, 19.9461044
      },

      // Expected slope of svp - temperature curve [kPa / K]
      check_svp_to_t,
      expected_svp_to_T[] = {
        0.0039537, 0.0099076, 0.0230775, 0.0503666,
        0.0822986, 0.1449156, 0.2437929, 0.3937122, 0.6129093, 0.9231149
      };

    for (i = 0; i < 10; i++)
    {
      check_svp = svp(temp_C[i], &check_svp_to_t);

      EXPECT_NEAR(check_svp, expected_svp[i], tol6);
      EXPECT_NEAR(check_svp_to_t, expected_svp_to_T[i], tol6);
    }
  }




  // Test `petfunc()`
  TEST(AtmDemandTest, PETpetfunc)
  {
    SW_ATMD SW_AtmDemand;

    LOG_INFO LogInfo;
    sw_init_logs(NULL, &LogInfo); // Initialize logs and silence warn/error reporting


    int i;
    unsigned int
      doy = 2,
      desc_rsds = 0;
    double
      check_pet,
      rsds = SW_MISSING,
      H_gt, H_oh, H_ot, H_gh,
      lat = 39. * deg_to_rad,
      elev = 1000.,
      slope0 = 0.,
      sloped = 5. * deg_to_rad,
      aspect = -90. * deg_to_rad, // East-facing slope
      reflec = 0.15,
      temp = 25.,
      RH = 61.,
      windsp = 1.3,
      cloudcov = 71.,
      actual_vap_pressure;


    // TEST `petfunc()` for varying average daily air temperature `avgtemp` [C]
    double
      // Inputs
      avgtemps[] = {-30, -20, -10, 0, 10, 20, 30, 40, 50, 60},
      // Expected PET
      expected_pet_avgtemps[] = {
        0.0100, 0.0184, 0.0346, 0.0576,
        0.0896, 0.1290, 0.1867, 0.2736, 0.4027, 0.5890
      };

    SW_PET_init_run(&SW_AtmDemand); // Init radiation memoization

    for (i = 0; i < 10; i++)
    {
      actual_vap_pressure = actualVaporPressure1(RH, avgtemps[i]);

      H_gt = solar_radiation(
        &SW_AtmDemand, doy,
        lat, elev, slope0, aspect, reflec,
        &cloudcov, actual_vap_pressure,
        rsds, desc_rsds,
        &H_oh, &H_ot, &H_gh, &LogInfo
      );
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      check_pet = petfunc(H_gt, avgtemps[i], elev, reflec, RH, windsp,
                          cloudcov, &LogInfo);
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      EXPECT_NEAR(check_pet, expected_pet_avgtemps[i], tol3);
    }


    // TEST `petfunc()` for varying latitude `lat` [± pi / 2]
    double
      // Inputs
      lats[] = {-90., -45., 0., 45., 90.},
      // Expected PET
      expected_pet_lats[] = {0.416576, 0.435964, 0.359670, 0.121564, 0.042131};

    double e_a = actualVaporPressure1(RH, temp);

    for (i = 0; i < 5; i++)
    {
      SW_PET_init_run(&SW_AtmDemand); // Re-init radiation memoization

      H_gt = solar_radiation(
        &SW_AtmDemand, doy,
        lats[i] * deg_to_rad, elev, slope0, aspect, reflec,
        &cloudcov, e_a,
        rsds, desc_rsds,
        &H_oh, &H_ot, &H_gh, &LogInfo
      );
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      check_pet = petfunc(H_gt, temp, elev, reflec, RH, windsp,
                          cloudcov, &LogInfo);
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      EXPECT_NEAR(check_pet, expected_pet_lats[i], tol6);
   }


    // TEST `petfunc()` for varying elevation [m a.s.l.]
    // Testing from -413 meters (Death Valley) to 8727 meters (~Everest).
    double
      // Inputs
      elevs[] = {-413, 0, 1000, 4418, 8727},
      // Expected PET
      expected_pet_elevs[] = {0.1670, 0.1634, 0.1550, 0.1305, 0.1093};

    for (i = 0; i < 5; i++)
    {
      SW_PET_init_run(&SW_AtmDemand); // Re-init radiation memoization

      H_gt = solar_radiation(
        &SW_AtmDemand, doy,
        lat, elevs[i], slope0, aspect, reflec,
        &cloudcov, e_a,
        rsds, desc_rsds,
        &H_oh, &H_ot, &H_gh, &LogInfo
      );
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      check_pet = petfunc(H_gt, temp, elevs[i], reflec, RH, windsp,
                          cloudcov, &LogInfo);
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      EXPECT_NEAR(check_pet, expected_pet_elevs[i], tol3);
    }


    // TEST `petfunc()` for varying slope [0 - pi / 2; radians]
    double
      // Inputs
      slopes[] = {0., 15., 34., 57., 90.},
      // Expected PET
      expected_pet_slopes[] = {0.1550, 0.1542, 0.1512, 0.1429, 0.1200};

    for (i = 0; i < 5; i++)
    {
      SW_PET_init_run(&SW_AtmDemand); // Re-init radiation memoization

      H_gt = solar_radiation(
        &SW_AtmDemand, doy,
        lat, elev, slopes[i] * deg_to_rad, aspect, reflec,
        &cloudcov, e_a,
        rsds, desc_rsds,
        &H_oh, &H_ot, &H_gh, &LogInfo
      );
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      check_pet = petfunc(H_gt, temp, elev, reflec, RH, windsp,
                          cloudcov, &LogInfo);
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      EXPECT_NEAR(check_pet, expected_pet_slopes[i], tol3);
    }


    // TEST `petfunc()` for varying aspect
    //   [South facing slope = 0, East = -pi / 2, West = pi / 2, North = ±pi]
    double
      // Inputs
      aspects[] = {-180, -90, -45, 0, 45, 90, 180},
      // Expected PET
      expected_pet_aspects[] = {
        0.1357, 0.1549, 0.1681, 0.1736, 0.1681, 0.1549, 0.1357
      };

    for (i = 0; i < 7; i++)
    {
      SW_PET_init_run(&SW_AtmDemand); // Re-init radiation memoization

      H_gt = solar_radiation(
        &SW_AtmDemand, doy,
        lat, elev, sloped, aspects[i] * deg_to_rad, reflec,
        &cloudcov, e_a,
        rsds, desc_rsds,
        &H_oh, &H_ot, &H_gh, &LogInfo
      );
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      check_pet = petfunc(H_gt, temp, elev, reflec, RH, windsp,
                          cloudcov, &LogInfo);
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      EXPECT_NEAR(check_pet, expected_pet_aspects[i], tol3);
    }


    // TEST `petfunc()` for varying albedo [0-1]
    double
      // Inputs
      reflecs[] = {0., 0.22, 0.46, 0.55, 1.},
      // Expected PET
      expected_pet_reflecs[] = {0.1745, 0.1457, 0.1141, 0.1022, 0.0421};

    for (i = 0; i < 5; i++)
    {
      SW_PET_init_run(&SW_AtmDemand); // Re-init radiation memoization

      H_gt = solar_radiation(
        &SW_AtmDemand, doy,
        lat, elev, sloped, aspect, reflecs[i],
        &cloudcov, e_a,
        rsds, desc_rsds,
        &H_oh, &H_ot, &H_gh, &LogInfo
      );
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      check_pet = petfunc(H_gt, temp, elev, reflecs[i], RH, windsp,
                          cloudcov, &LogInfo);
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      EXPECT_NEAR(check_pet, expected_pet_reflecs[i], tol3);
    }


    // TEST `petfunc()` for varying relative humidity [0-100; %]
    double
      // Inputs
      RHs[] = {0, 34, 56, 79, 100},
      // Expected PET
      expected_pet_RHs[] = {0.2267, 0.2123, 0.1662, 0.1128, 0.0612};

    for (i = 0; i < 5; i++)
    {
      SW_PET_init_run(&SW_AtmDemand); // Re-init radiation memoization

      actual_vap_pressure = actualVaporPressure1(RHs[i], temp);

      H_gt = solar_radiation(
        &SW_AtmDemand, doy,
        lat, elev, slope0, aspect, reflec,
        &cloudcov, actual_vap_pressure,
        rsds, desc_rsds,
        &H_oh, &H_ot, &H_gh, &LogInfo
      );
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      check_pet = petfunc(H_gt, temp, elev, reflec, RHs[i], windsp,
                          cloudcov, &LogInfo);
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      EXPECT_NEAR(check_pet, expected_pet_RHs[i], tol3);
    }


    // TEST `petfunc()` for varying wind speed [m / s]
    double
      // Inputs
      windsps[] = {0., 1., 5., 10., 20.},
      // Expected PET
      expected_pet_windsps[] = {0.1016, 0.1426, 0.3070, 0.5124, 0.9232};

    SW_PET_init_run(&SW_AtmDemand); // Re-init radiation memoization

    H_gt = solar_radiation(
      &SW_AtmDemand, doy,
      lat, elev, slope0, aspect, reflec,
      &cloudcov, e_a,
      rsds, desc_rsds,
      &H_oh, &H_ot, &H_gh, &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (i = 0; i < 5; i++)
    {
      check_pet = petfunc(H_gt, temp, elev, reflec, RH, windsps[i],
                          cloudcov, &LogInfo);
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      EXPECT_NEAR(check_pet, expected_pet_windsps[i], tol3);
    }


    // TEST `petfunc()` for varying cloud cover [0-100; %]
    double
      // Inputs
      cloudcovs[] = {0, 12, 36, 76, 100},
      // Expected PET
      expected_pet_cloudcovs[] = {0.1253, 0.1303, 0.1404, 0.1571, 0.1671};
      // Note: increasing cloud cover decreases H_gt and increases PET

    for (i = 0; i < 5; i++)
    {
      SW_PET_init_run(&SW_AtmDemand); // Re-init radiation memoization

      H_gt = solar_radiation(
        &SW_AtmDemand, doy,
        lat, elev, slope0, aspect, reflec,
        &cloudcovs[i], e_a,
        rsds, desc_rsds,
        &H_oh, &H_ot, &H_gh, &LogInfo
      );
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      check_pet = petfunc(H_gt, temp, elev, reflec, RH, windsp,
                          cloudcovs[i], &LogInfo);
      sw_fail_on_error(&LogInfo); // exit test program if unexpected error

      EXPECT_NEAR(check_pet, expected_pet_cloudcovs[i], tol3);
    }
  }



  #ifdef SW2_PET_Test__petfunc_by_temps
  // Run SOILWAT2 unit tests with flag
  // ```
  //   CPPFLAGS=-DSW2_PET_Test__petfunc_by_temps make test test_run
  // ```
  //
  // Produce plots based on output generated above
  // ```
  //   Rscript tools/plot__SW2_PET_Test__petfunc_by_temps.R
  // ```
  TEST(AtmDemandTest, PETPetfuncByTemps)
  {
    SW_ATMD SW_AtmDemand;
    SW_PET_init_run(&SW_AtmDemand); // Init radiation memoization

    LOG_INFO LogInfo;
    sw_init_logs(NULL, &LogInfo); // Initialize logs and silence warn/error reporting


    int doy, k1, k2, k3, k4, k5;

    unsigned int desc_rsds = 0;

    double
      pet,
      temp, RH, windspeed, cloudcover, fH_gt,
      rsds = SW_MISSING,
      H_gt, H_oh, H_ot, H_gh,
      elev = 0.,
      lat = 40.,
      slope = 0.,
      aspect = SW_MISSING,
      reflec = 0.15;

    FILE *fp;
    char fname[FILENAME_MAX];

    strcpy(fname, output_prefix);
    strcat(fname, "Table__SW2_PET_Test__petfunc_by_temps.csv");
    fp = OpenFile(fname, "w");

    // Column names
    fprintf(
      fp,
      "Temperature_C, RH_pct, windspeed_m_per_s, cloudcover_pct, fH_gt, PET_mm"
      "\n"
    );

    // Loop over treatment factors
    for (k1 = -40; k1 < 60; k1++)
    {
      temp = k1;

      for (k2 = 0; k2 <= 10; k2++)
      {
        RH = 10. * k2;

        for (k3 = 0; k3 <= 3; k3++)
        {
          windspeed = squared(k3);

          for (k4 = 0; k4 <= 3; k4++)
          {
            cloudcover = 33.3 * k4;

            for (k5 = -1; k5 <= 1; k5++)
            {
              fH_gt = 1 + k5 * 0.2;
              pet = 0.;

              for (doy = 1; doy <= 365; doy++)
              {

                H_gt = fH_gt * solar_radiation(
                  &SW_AtmDemand, doy,
                  lat, elev, slope, aspect, reflec,
                  &cloudcover, RH, temp,
                  rsds, desc_rsds,
                  &H_oh, &H_ot, &H_gh, &LogInfo
                );
                sw_fail_on_error(&LogInfo); // exit test program if unexpected error

                pet += petfunc(
                  H_gt,
                  temp,
                  elev, reflec,
                  RH, windspeed, cloudcover,
                  &LogInfo
                );
                sw_fail_on_error(&LogInfo); // exit test program if unexpected error
              }

              fprintf(
                fp,
                "%f, %f, %f, %f, %f, %f\n",
                temp,
                RH,
                windspeed,
                cloudcover,
                fH_gt,
                pet
              );

              fflush(fp);
            }
          }
        }
      }
    }

    // Clean up
    CloseFile(&fp);
  }
  #endif // end of SW2_PET_Test__petfunc_by_temps


}
