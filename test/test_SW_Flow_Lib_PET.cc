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
#include <sstream>
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


  // Test sun hour angles for horizontal and tilted surfaces
  TEST(SW2_SolarPosition_Test, hourangles_symmetries)
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
    std::ostringstream msg;


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
                  sw_error(
                    -1,
                    "Error in SW2_SolarPosition_Test__hourangles_symmetries"
                  );
              }

              // Calculate sun hour angles
              sun_hourangles(
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

            SW_PET_init_run(); // Re-init radiation memoization
          }


          for (itime = 0; itime < 14; itime++) {
            msg.str("");
            msg <<
              " doy = " << doy_used[1][itime] <<
              ", lat = " <<
                round(latitude_used[1][itime] * rad_to_deg * 100.) / 100. <<
              ", slope = " <<
                round(slope * rad_to_deg * 100.) / 100. <<
              ", aspect = " <<
                round(aspect_used[0][itime] * rad_to_deg * 100.) / 100. <<
              "|" <<
                round(aspect_used[1][itime] * rad_to_deg * 100.) / 100.;

            //------ Expectation 2: Daylength:
            // symmetric in aspect reflected around South aspect: 0±abs(asp)
            EXPECT_NEAR(daylength[0][itime], daylength[1][itime], tol9) <<
              "symmetry (reflected aspect) of daylength for" <<
              msg.str();

            //------ Expectation 3: Tilted sunrise/sunset:
            // negatively symmetric in aspect reflected around South aspect
            for (k2 = 0; k2 < 4; k2++) {
              if (
                !missing(o[0][itime][2 + k2]) ||
                !missing(o[1][itime][5 - k2])
              ) {
                EXPECT_NEAR(
                  o[0][itime][2 + k2],
                  -o[1][itime][5 - k2],
                  tol9
                ) <<
                  "symmetry (reflected aspect) of tilted sunrise/sunset for" <<
                  msg.str();
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
  TEST(SW2_SolarPosition_Test, hourangles_by_lat_and_doy)
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
  TEST(SW2_SolarPosition_Test, hourangles_by_lats)
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
  TEST(SW2_SolarRadiation_Test, extraterrestrial)
  {
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
        if (std::isfinite(H_oh_Table1_10_1[k1][k2])) {
          doy = doys_Table1_6_1[k2];

          sun_hourangles(
            doy, lat, 0., 0.,
            sun_angles, int_cos_theta, int_sin_beta
          );
          solar_radiation_extraterrestrial(doy, int_cos_theta, H_o);

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

      SW_PET_init_run(); // Re-init radiation memoization (for next location)
    }


    // Duffie & Beckman 2013: Example 1.10.1
    doy = 105;
    sun_hourangles(
      doy, lat_Madison_WI, 0., 0.,
      sun_angles, int_cos_theta, int_sin_beta
    );
    solar_radiation_extraterrestrial(doy, int_cos_theta, H_o);
    EXPECT_NEAR(H_o[0], 33.8, 2. * tol1)
      << "Duffie & Beckman 2013: Example 1.10.1\n";
    SW_PET_init_run(); // Re-init radiation memoization

    // Duffie & Beckman 2013: Example 2.11.1
    doy = 246;
    sun_hourangles(
      doy, lat_StLouis_MO, 0., 0.,
      sun_angles, int_cos_theta, int_sin_beta
    );
    solar_radiation_extraterrestrial(doy, int_cos_theta, H_o);
    EXPECT_NEAR(H_o[0], 33.0, 7. * tol1)
      << "Duffie & Beckman 2013: Example 2.11.1\n";
    SW_PET_init_run(); // Re-init radiation memoization

    // Duffie & Beckman 2013: Example 2.12.1
    doy = 162;
    sun_hourangles(
      doy, lat_Madison_WI, 0., 0.,
      sun_angles, int_cos_theta, int_sin_beta
    );
    solar_radiation_extraterrestrial(doy, int_cos_theta, H_o);
    EXPECT_NEAR(H_o[0], 41.8, tol1)
      << "Duffie & Beckman 2013: Example 2.12.1\n";
    SW_PET_init_run(); // Re-init radiation memoization
  }


  // Test solar radiation: global horizontal and tilted
  //   Comparison against examples by Duffie & Beckman 2013 are expected to
  //   deviate in value, but show similar patterns, because (i) calculations for
  //   H_oh differ (see `SW2_SolarRadiation_Test.extraterrestrial`), (ii)
  //   we calculate H_gh while they use measured H_gh values, and (iii)
  //   separation models differ, etc.
  TEST(SW2_SolarRadiation_Test, global)
  {
    unsigned int k;

    // Duffie & Beckman 2013: Table 1.6.1
    unsigned int doys_Table1_6_1[12] = {
      17, 47, 75, 105, 135, 162, 198, 228, 258, 288, 318, 344
    };
    double
      H_gt, H_ot, H_oh, H_gh,
      // Duffie & Beckman 2013: Example 2.19.1
      H_Ex2_19_1[3][12] = {
        // H_oh
        {13.37, 18.81, 26.03, 33.78, 39.42, 41.78, 40.56, 35.92, 28.80, 20.90, 14.62, 11.91},
        // H_gh
        {6.44, 9.89, 12.86, 16.05, 21.36, 23.04, 22.58, 20.33, 14.59, 10.48, 6.37, 5.74},
        // H_gt
        {13.7, 17.2, 15.8, 14.7, 16.6, 16.5, 16.8, 17.5, 15.6, 15.2, 11.4, 12.7}
      },
      albedo[12] =
        {0.7, 0.7, 0.4, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.4},
      // Climate normals for Madison, WI
      // "WMO Climate Normals for MADISON/DANE CO REGIONAL ARPT, WI 1961–1990".
      // National Oceanic and Atmospheric Administration. Retrieved March 10, 2014.
      // ftp://ftp.atdd.noaa.gov/pub/GCOS/WMO-Normals/TABLES/REG_IV/US/GROUP4/72641.TXT
      cloud_cover[12] =
        // Element 20:  Sky Cover (Cloud Cover)
        // {66.25, 66.25, 70, 67.5, 65, 60, 57.5, 57.5, 60, 63.75, 72.5, 71.25},
        // Mar, Apr, Sep, Oct, Nov, Dec: replaced observed with estimated values to match `H_Ex2_19_1`
        {66.25, 66.25, 80., 90., 65.0, 60.0, 57.5, 57.5, 80., 75., 85., 60.},
      // Element 11:  Relative Humidity (%), MN3HRLY (Statistic 94):  Mean of 3-Hourly Observations
      rel_humidity[12] =
        {74.5, 73.1, 71.4, 66.3, 65.8, 68.3, 71.0, 74.4, 76.8, 73.2, 76.9, 78.5},
      // Element 01:  Dry Bulb Temperature (deg C)
      air_temp_mean[12] =
        {-8.9, -6.3, 0.2, 7.4, 13.6, 19, 21.7, 20.2, 15.4, 9.4, 1.9, -5.7};

    // Duffie & Beckman 2013: Example 2.19.1
    for (k = 0; k < 12; k++) {
      H_gt = solar_radiation(
        doys_Table1_6_1[k],
        43. * deg_to_rad, // latitude
        226., // elevation
        60 * deg_to_rad, // slope
        0., // aspect
        albedo[k],
        cloud_cover[k],
        rel_humidity[k],
        air_temp_mean[k],
        &H_oh,
        &H_ot,
        &H_gh
      );

      EXPECT_NEAR(H_oh, H_Ex2_19_1[0][k], tol0)
        << "Duffie & Beckman 2013: Example 2.19.1, H_oh: "
        << "month = " << k + 1 << "\n";
      EXPECT_NEAR(H_gh, H_Ex2_19_1[1][k], tol0)
        << "Duffie & Beckman 2013: Example 2.19.1, H_gh: "
        << "month = " << k + 1 << "\n";
      EXPECT_NEAR(H_gt, H_Ex2_19_1[2][k], tol0)
        << "Duffie & Beckman 2013: Example 2.19.1, H_gt: "
        << "month = " << k + 1 << "\n";
    }

    SW_PET_init_run(); // Re-init radiation memoization
  }




  // Test saturated vapor pressure function
  TEST(SW2_PET_Test, svapor)
  {
    int i;
    double
      // Temperature [C]
      temp_C[] = {-30, -20, -10, 0, 10, 20, 30, 40, 50, 60},

      // Expected saturated vapor pressure
      expected_svp[] = {
        0.3889344, 0.9389376, 2.1197755,
        4.5085235,
        9.0911046, 17.4746454, 32.1712519, 56.9627354, 97.3531630, 161.1126950
      };

    for (i = 0; i < 10; i++)
    {
      EXPECT_NEAR(svapor(temp_C[i]), expected_svp[i], tol6);
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
