/********************************************************/
/********************************************************/
/*
  Source file: SW_Flow_lib_PET.c
  Type: module
  Application: SOILWAT2 - soilwater dynamics simulator
  Purpose: Radiation and evaporative demand
*/
/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include <math.h>
#include "include/generic.h"
#include "include/SW_Defines.h"

#include "include/SW_Flow_lib_PET.h"



/* =================================================== */
/*                  Local Variables                    */
/* --------------------------------------------------- */

static double
  memoized_G_o[366][2],
  msun_angles[366][7],
  memoized_int_cos_theta[366][2],
  memoized_int_sin_beta[366][2];


/** @brief Solar constant

  Solar constant = 1367 [W / m2], World Radiation Center (WRC)
  convert to daily irradiance:
     - 24 * 60 * 60 [s/day] convert W/m2 -> J/day/m2
     - 1e-6 convert J -> MJ
   G_sc [MJ / (m2 * day)]
*/
static const double G_sc = 118.1088;


#define has_tilted_surface(slope, aspect) \
  ( GT( (slope), 0. ) && !missing( (aspect) ) )



/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/** @brief Initialize global and memoized variables
*/
void SW_PET_init_run(void) {
  int k1, k2;

  for (k1 = 0; k1 < 366; k1++)
  {
    memoized_G_o[k1][0] = memoized_G_o[k1][1] = SW_MISSING;
    memoized_int_cos_theta[k1][0] = memoized_int_cos_theta[k1][1] = 0.;
    memoized_int_sin_beta[k1][0] = memoized_int_sin_beta[k1][1] = 0.;

    for (k2 = 0; k2 < 7; k2++)
    {
      msun_angles[k1][k2] = SW_MISSING;
    }
  }
}



/** @brief Relative sun-earth distance

  Formula is based on Spencer 1971 @cite Spencer1971
  as cited by Duffie & Beckman 2013 @cite duffie2013.
  Reported error is within ±0.01%.

  @param doy Day of year [1-365]

  @return d^-2 where d = relative sun-earth distance [au].
*/
double sun_earth_distance_squaredinverse(unsigned int doy)
{
  double
    dayAngle = swPI2 / 365.25 * (doy - 0.5);

  // Spencer 1971; Duffie & Beckman 2013: eq. 1.4.1b
  return 1.000110 \
      + 0.034221 * cos(dayAngle) \
      + 0.001280 * sin(dayAngle) \
      + 0.000719 * cos(2. * dayAngle) \
      + 0.000077 * sin(2. * dayAngle);
}

/** @brief Solar declination

  Formula is from Spencer 1971 @cite Spencer1971.
  Errors are reported to be ±0.0007 radians (0.04 degrees);
  except near equinoxes in leap years when errors can be twice as large.

  Solar declination is 0 at spring and fall equinoxes,
  positive during the Northern/Southern Hemisphere summer/winter,
  and negative during the Northern/Southern Hemisphere winter/summer.

  @param doy Day of year [1-365]

  @return Solar declination angle [radians].
*/
double solar_declination(unsigned int doy)
{
  /* Notes: equation used previous to Oct/11/2012 (from unknown source):
      declin = .401426 *sin(6.283185 *(doy -77.) /365.);
  */

  double
    dayAngle = swPI2 / 365.25 * (doy - 0.5);

  // Spencer (1971): declin = solar declination [radians]
  return 0.006918 \
    - 0.399912 * cos(dayAngle) \
    + 0.070257 * sin(dayAngle) \
    - 0.006758 * cos(2. * dayAngle) \
    + 0.000907 * sin(2. * dayAngle) \
    - 0.002697 * cos(3. * dayAngle) \
    + 0.001480 * sin(3. * dayAngle);
}



/** @brief Sunset/sunrise hour angle on a horizontal surface

  Hour angle values are in the range [-pi, +pi];
  they are negative from sunrise omega = -omega_s to omega = 0 at solar noon
  and are positive from solar noon to sunset omega = omega_s.

  Equation based on Duffie & Beckman 2013 @cite duffie2013.

  @param lat Latitude of the site [radians].
  @param declin Solar declination [radians].

  @return Sunset (or -sunrise) hour angle \f$ \omega_s \f$ [radians] on a
    horizontal surface.
*/
double sunset_hourangle(double lat, double declin)
{
  // based on Duffie & Beckman 2013: eq. 1.6.10
  return acos(fmax(-1.0, fmin(1.0, -tan(lat) * tan(declin))));
}


/** @brief Integrals of solar incidence angle and solar altitude angle

  This function calculates all possible sunrise and sunset hour angles
  for a horizontal and a tilted surface for all latitudes, slopes, and aspects
  based on equations developed by Allen et al. 2006 @cite allen2006AaFM.

  Note that Duffie & Beckman 2013 @cite duffie2013 present an alternative
  set of equations to calculate sunrise and sunset hour angles. While these
  provide comparable outcomes to the Allen et al. equations
  for sun-facing conditions, they produce, as implemented here, much more
  self-shading and no two-period daylight outcomes. They are activated
  if `sun_hourangles_Duffie2013_2205` is defined, e.g.,
  `CPPFLAGS=-Dsun_hourangles_Duffie2013_2205 make bin`

  @note The function is memoized on `doy`.
    The other arguments latitude, slope, and aspect are fixed
    for any given SOILWAT2.
    Call \ref SW_PET_init_run to initialize the memoization.

  @note The 2nd elements of the return arrays
    `int_cos_theta` and `int_sin_beta` provide horizontal values
    if the input surface, defined by `slope` and `aspect`, is horizontal.


  @param[in] doy Day of year [1-365].
  @param[in] lat Latitude of the site [radians].
  @param[in] slope Slope of the site
    between 0 (horizontal) and pi / 2 (vertical) [radians].
  @param[in] aspect Surface aspect of the site [radians].
    A value of \ref SW_MISSING indicates no data, i.e., treat it as if slope = 0;
    South facing slope: aspect = 0, East = -pi / 2, West = pi / 2, North = ±pi.

  @param[out] sun_angles Array of size 7 where the elements are as follows:

      1. indicator:
         - sun never rises above horizontal surface = -2
         - sun never rises above tilted surface = -1
         - sun never sets = 0
         - sun rises and sets once for tilted surface = 1
         - sun rises and sets twice for tilted surface = 2
      2. sunrise hour angle on horizontal surface -omega_s
         (equal to the negative of the sunset hour angle on horizontal surface)
      3. first sunrise hour angle on tilted surface
      4. first sunset hour angle on tilted surface (if two periods of sunshine)
      5. second sunrise hour angle on tilted surface (if two periods of sunshine)
      6. final sunset hour angle on tilted surface
      7. sunset hour angle on horizontal surface omega_s

  @param[out] int_cos_theta Array of length 2 with
    daily integral during sunshine (one or two periods)
    of the cosine of the solar incidence angle
    on a horizontal (1st element) and tilted (2nd element) surface [-].

  @param[out] int_sin_beta Array of length 2 with
    daily integral during sunshine (one or two periods)
    of the sine of the sun angle (= solar altitude angle)
    above a horizontal (1st element) and tilted (2nd element) surface [-]
*/
void sun_hourangles(unsigned int doy, double lat, double slope, double aspect,
 double sun_angles[], double int_cos_theta[], double int_sin_beta[])
{
  unsigned int
    i,
    doy0 = doy - 1; // doy is base1


  if (missing(msun_angles[doy0][0])) {

    // (drs) set tolerance to 1e-9 instead of 1e-3 to minimize "edge" effects
    static const double tol = 1e-9;

    double
      declin,
      a, b, c, g, h,
      f1, f2, f3, f4, f5,
      tmp, tmp1, tmp2, tmp3, X,
      omega1, trig_omega1, omega1b, cos_theta1b,
      omega2, trig_omega2, omega2b, cos_theta2b,
      cos_theta_sunrise, cos_theta_sunset;


    // Calculate solar declination
    declin = solar_declination(doy);


    //------ Horizontal surfaces: sunset and sunrise angles
    if ((declin + lat > swPI_half) || (declin + lat < -swPI_half)) {
      // sun never sets
      msun_angles[doy0][0] = 0;
      msun_angles[doy0][6] = msun_angles[doy0][5] = swPI;

    } else if ((declin - lat > swPI_half) || (declin - lat < -swPI_half)) {
      // sun never rises
      msun_angles[doy0][0] = -2;
      msun_angles[doy0][6] = msun_angles[doy0][5] = 0.;

    } else {
      msun_angles[doy0][0] = 1;
      msun_angles[doy0][6] = msun_angles[doy0][5] = sunset_hourangle(lat, declin);
    }


    // Sunrise hour angle on horizontal surface is
    // equal to the negative of the sunset hour angle
    msun_angles[doy0][1] = -msun_angles[doy0][6];
    msun_angles[doy0][2] = -msun_angles[doy0][5];


    // Check that we have daylight
    if (GE(msun_angles[doy0][0], 0.)) {

      //------ Integrate on a horizontal surface from sunrise to sunset
      g = sin(declin) * sin(lat);
      h = cos(declin) * cos(lat);


      // Integrate the sine of the sun angle (= solar altitude angle)
      // above a horizontal surface from sunrise to sunset
      tmp1 = 2. * squared(g) * msun_angles[doy0][6] \
           + 4. * g * h * sin(msun_angles[doy0][6]) \
           + squared(h) *
             (msun_angles[doy0][6] + 0.5 * sin(2. * msun_angles[doy0][6]));
      tmp2 = g * msun_angles[doy0][6] + h * sin(msun_angles[doy0][6]);

      // Allen et al. 2006: eq. 26
      memoized_int_sin_beta[doy0][0] = fmax(0., tmp1 / (2. * tmp2));


      // Integrate the cosine of the solar incidence angle on a
      // horizontal surface from sunrise to sunset: Allen et al. 2006: eq. 35
      // (drs): re-expressed eq. 35 with `g` and `h` via eq. 26
      // (drs): standardize by pi because integral is for half day (0 to sunset)
      memoized_int_cos_theta[doy0][0] = tmp2 / swPI;



      //------ Tilted surfaces: sunset and sunrise angles
      if (has_tilted_surface(slope, aspect)) {

        // Calculate auxiliary variables
        a = sin(declin) * (
              cos(lat) * sin(slope) * cos(aspect) -
              sin(lat) * cos(slope)
            );
        b = cos(declin) * (
              sin(lat) * sin(slope) * cos(aspect) +
              cos(lat) * cos(slope)
            );
        c = cos(declin) * sin(slope) * sin(aspect);


        // Calculate angle of indicence on tilted surface at
        // horizontal sunrise and sunrise hour angles
        cos_theta_sunrise =
          -a + b * cos(msun_angles[doy0][1]) + c * sin(msun_angles[doy0][1]);
        cos_theta_sunset =
          -a + b * cos(msun_angles[doy0][6]) + c * sin(msun_angles[doy0][6]);


        // Calculate candidate sunrise and sunset hour angles
        // on tilted surface (Eq. 11-13)
        tmp3 = squared(b) + squared(c);
        tmp = tmp3 - squared(a);
        tmp = (tmp > 0.) ? sqrt(tmp) : tol;

        // Calculation of omega via asin(sin(omega)) as per Eq. 12-13
        tmp1 = a * c;
        tmp2 = b * tmp;

        trig_omega1 = (tmp1 - tmp2) / tmp3;
        // see Step Diii (Appendix A)
        trig_omega1 = fmax(-1. + tol, fmin(1. - tol, trig_omega1));
        omega1 = asin(trig_omega1);

        trig_omega2 = (tmp1 + tmp2) / tmp3;
        // see Step Diii (Appendix A)
        trig_omega2 = fmax(-1. + tol, fmin(1. - tol, trig_omega2));
        omega2 = asin(trig_omega2);



      #ifndef sun_hourangles_Duffie2013_2205
        // Use Allen et al. 2006 to calculate tilted sunrise/sunset

        double
          omega1x, cos_theta1, cos_theta1x,
          omega2x, cos_theta2, cos_theta2x;

        // Candidate sunrise and sunset hour angles on tilted surface
        cos_theta1 = -a + b * cos(omega1) + c * sin(omega1);
        cos_theta2 = -a + b * cos(omega2) + c * sin(omega2);


        // Step B. Determine the beginning integration limit representing
        // the initial incidence of the center of the solar beam on the slope
        if (
          ((cos_theta_sunrise < cos_theta1) ||
          EQ_w_tol(cos_theta_sunrise, cos_theta1, tol)) &&
          (cos_theta1 < tol)
        ) {

          // Sunrise on tilted surface is after sunrise on horizontal surface
          msun_angles[doy0][2] = omega1;

        } else {
          // Sunsrise occurs with the sun above the tilted surface
          omega1x = -swPI - omega1;
          cos_theta1x = -a + b * cos(omega1x) + c * sin(omega1x);

          if (
            (cos_theta1x > tol) ||
            ((cos_theta1x <= tol) &&
            ((omega1x < msun_angles[doy0][1]) ||
            EQ_w_tol(omega1x, msun_angles[doy0][1], tol)))
          ) {
            msun_angles[doy0][2] = msun_angles[doy0][1];

          } else {
            msun_angles[doy0][2] = omega1x;
          }
        }

        if (msun_angles[doy0][2] < msun_angles[doy0][1]) {
          // prevent "transparent" earth
          msun_angles[doy0][2] = msun_angles[doy0][1];
        }


        // Step C. Determine the ending integration limit representing
        // the final incidence of the center of the solar beam on the slope
        if (
          ((cos_theta_sunset < cos_theta2) ||
          EQ_w_tol(cos_theta_sunset, cos_theta2, tol)) &&
          (cos_theta2 < tol)
        ) {

          // Sunrise on tilted surface is before sunset on horizontal surface
          msun_angles[doy0][5] = omega2;

        } else {
          // Sunset occurs with the sun above the tilted surface
          omega2x = swPI - omega2;
          cos_theta2x = -a + b * cos(omega2x) + c * sin(omega2x);

          if (
            (cos_theta2x > tol) ||
            ((cos_theta2x <= tol) &&
            ((omega2x > msun_angles[doy0][6]) ||
            EQ_w_tol(omega2x, msun_angles[doy0][6], tol)))
          ) {
            msun_angles[doy0][5] = msun_angles[doy0][6];

          } else {
            msun_angles[doy0][5] = omega2x;
          }
        }

        if (msun_angles[doy0][5] > msun_angles[doy0][6]) {
          // prevent "transparent" earth
          msun_angles[doy0][5] = msun_angles[doy0][6];
        }


      #else
        // instead of Allen et al. 2006 use
        // Duffie et al. 2013: eq. 2.20.5e-2.20.5i

        double A, B, C, abs_omega_sr, abs_omega_ss;

        A = cos(slope) + tan(lat) * cos(aspect) * sin(slope);
        B = cos(msun_angles[doy0][6]) * cos(slope) + \
          tan(declin) * sin(slope) * cos(aspect);

        if (ZRO(cos(lat))) {
          C = sin(slope) * sin(aspect) / tol;
        } else {
          C = sin(slope) * sin(aspect) / cos(lat);
        }

        tmp3 = squared(A) + squared(C);
        tmp = tmp3 - squared(B);
        tmp = (tmp > 0.) ? sqrt(tmp) : tol;

        tmp1 = A * B;
        tmp2 = C * tmp;

        trig_omega1 = (tmp1 + tmp2) / tmp3;
        trig_omega1 = fmax(-1. + tol, fmin(1. - tol, trig_omega1));
        abs_omega_sr = fmin(msun_angles[doy0][6], acos(trig_omega1));

        trig_omega2 = (tmp1 - tmp2) / tmp3;
        trig_omega2 = fmax(-1. + tol, fmin(1. - tol, trig_omega2));
        abs_omega_ss = fmin(msun_angles[doy0][6], acos(trig_omega2));

        if ((A > 0. && B > 0.) || (A > B) || EQ_w_tol(A, B, tol)) {
          msun_angles[doy0][2] = -abs_omega_sr;
          msun_angles[doy0][5] = abs_omega_ss;

        } else {
          msun_angles[doy0][2] = abs_omega_sr;
          msun_angles[doy0][5] = -abs_omega_ss;
        }

      #endif


        // Step D. Additional limits for numerical stability and
        // twice per day periods of sun

        if (msun_angles[doy0][2] >= msun_angles[doy0][5]) {
          // slope is always shaded
          msun_angles[doy0][0] = -1;
          msun_angles[doy0][2] = 0.;
          msun_angles[doy0][5] = 0.;
        }


        // Check that we have daylight
        if (GE(msun_angles[doy0][0], 0.)) {

          // Allen et al. 2006: eq. 7
          // (drs): take absolute of rhs so that this checks correctly not
          // only for Northern but also Southern latitudes
          tmp = sin(lat) * cos(declin) + cos(lat) * sin(declin);
          if (sin(slope) > fabs(tmp)) {

            // possibility for two periods of sunshine
            omega2b = fmin(omega1, omega2);
            omega1b = fmax(omega1, omega2);

            cos_theta1b = -a + b * cos(omega1b) + c * sin(omega1b);
            cos_theta2b = -a + b * cos(omega2b) + c * sin(omega2b);

            if (fabs(cos_theta1b) > tol) {
              omega1b = swPI - omega1b;
            }

            if (fabs(cos_theta2b) > tol) {
              omega2b = -swPI - omega2b;
            }

            if (
              ((omega2b > msun_angles[doy0][2]) ||
              EQ_w_tol(omega2b, msun_angles[doy0][2], tol)) &&
              ((omega1b < msun_angles[doy0][5]) ||
              EQ_w_tol(omega1b, msun_angles[doy0][5], tol))
            ) {
              // two periods of sunshine are still possible
              // Allen et al. 2006: eq. 50
              // (drs) re-expressed eq. 50 with `a`, `b`, and `c`
              X = - a * (omega1b - omega2b)
                  + b * (sin(omega1b) - sin(omega2b))
                  - c * (cos(omega1b) - cos(omega2b));

              if (LT(X, 0.)) {
                // indeed two periods of sunshine
                msun_angles[doy0][0] = 2;
                msun_angles[doy0][3] = omega2b;
                msun_angles[doy0][4] = omega1b;
              }
            }
          }


          // Integrate from (first) sunrise to (last) sunset
          if (EQ(msun_angles[doy0][0], 2.)) {
            // Two periods of sunshine
            f1 = sin(msun_angles[doy0][3]) - sin(msun_angles[doy0][2]) \
               + sin(msun_angles[doy0][5]) - sin(msun_angles[doy0][4]);
            f2 = cos(msun_angles[doy0][3]) - cos(msun_angles[doy0][2]) \
               + cos(msun_angles[doy0][5]) - cos(msun_angles[doy0][4]);
            f3 = msun_angles[doy0][3] - msun_angles[doy0][2] + \
                 msun_angles[doy0][5] - msun_angles[doy0][4];
            f4 = sin(2. * msun_angles[doy0][3]) \
               - sin(2. * msun_angles[doy0][2]) \
               + sin(2. * msun_angles[doy0][5]) \
               - sin(2. * msun_angles[doy0][4]);
            f5 = squared(sin(msun_angles[doy0][3])) \
               - squared(sin(msun_angles[doy0][2])) \
               + squared(sin(msun_angles[doy0][5])) \
               - squared(sin(msun_angles[doy0][4]));

          } else {
            // One period of sunshine
            f1 = sin(msun_angles[doy0][5]) - sin(msun_angles[doy0][2]);
            f2 = cos(msun_angles[doy0][5]) - cos(msun_angles[doy0][2]);
            f3 = msun_angles[doy0][5] - msun_angles[doy0][2];
            f4 = sin(2. * msun_angles[doy0][5]) \
               - sin(2. * msun_angles[doy0][2]);
            f5 = squared(sin(msun_angles[doy0][5])) \
               - squared(sin(msun_angles[doy0][2]));
          }


         // Integrate the sine of the sun angle (= solar altitude angle)
         // above tilted surface from (first) sunrise to (last) sunset
          tmp1 = f1 * (b * g - a * h) \
               - f2 * c * g \
               + f3 * (0.5 * b * h - a * g) \
               + f4 * 0.25 * b * h \
               + f5 * 0.5 * c * h;
          tmp2 = b * f1 - c * f2 - a * f3;
          //  Allen et al. 2006: eq. 22
          memoized_int_sin_beta[doy0][1] = fmax(0., tmp1 / tmp2);

          // Integrate the cosine of the solar incidence angle on tilted surface
          // from (first) sunrise to (last) sunset
          // Allen et al. 2006: eqs 5 (one sunshine period) and 51 (two periods)
          // (drs): re-expressed eq. 5 with `abc` and `f1-3` via eq. 22
          // (drs): standardize by 2 * pi because integral is across full day
          memoized_int_cos_theta[doy0][1] = tmp2 / swPI2;
        }

      } else {
        // Horizontal surface: grab horizontal values
        memoized_int_sin_beta[doy0][1] = memoized_int_sin_beta[doy0][0];
        memoized_int_cos_theta[doy0][1] = memoized_int_cos_theta[doy0][0];
      }
    }
  }

  // Grab memoized values
  int_cos_theta[0] = memoized_int_cos_theta[doy0][0];
  int_cos_theta[1] = memoized_int_cos_theta[doy0][1];
  int_sin_beta[0] = memoized_int_sin_beta[doy0][0];
  int_sin_beta[1] = memoized_int_sin_beta[doy0][1];

  for (i = 0; i < 7; i++) {
    sun_angles[i] = msun_angles[doy0][i];
  }
}



/** @brief Daily extraterrestrial solar irradiation

  This function approximates the origin of the solar beam as the center of
  the solar disk.
  The function corrects daily irradiation to the amount that would be received
  on a tilted surface on Earth in the absence of an atmosphere.

  @note The function is memoized on `doy`.
    The other argument `int_cos_theta` co-varies with `doy`
    for any given SOILWAT2 run where latitude, slope, and aspect are fixed.
    Call \ref SW_PET_init_run to initialize the memoization.

  @param[in] doy Day of year [1-365].
  @param[in] int_cos_theta Array of length 2 with
    daily integral during sunshine (one or two periods)
    of the cosine of the solar incidence angle
    on a horizontal (1st element) and tilted (2nd element) surface [-]

  @param[out] G_o Daily extraterrestrial solar irradiation
    incident on a horizontal plane at the top of the atmosphere [MJ / m2].
    G_o is an array of size 2 returning
    1. G_oh Horizontal irradiation without topographic correction
    2. G_ot Horizontal irradiation but corrected for sunshine duration
       on tilted surface (topographic correction)
*/
void solar_radiation_extraterrestrial(unsigned int doy, double int_cos_theta[],
  double G_o[])
{

  unsigned int doy0 = doy - 1; // doy is base1

  if (missing(memoized_G_o[doy0][0]))
  {
    int k;
    double di2 = 1.;

    // Calculate sun-earth distance effect
    if (GT(int_cos_theta[0], 0.) || GT(int_cos_theta[1], 0.)) {
      di2 = sun_earth_distance_squaredinverse(doy);
    }

    for (k = 0; k < 2; k++)
    {
      // k = 0: horizontal surface; k == 1: tilted surface

      // Check that we have sunshine
      if (GT(int_cos_theta[k], 0.)) {
        // Allen et al. 2006: eq. 35 (horizontal surface) and
        // eqs 6 and 51 (tilted surface)
        memoized_G_o[doy0][k] = G_sc * di2 * int_cos_theta[k];

      } else {
        // no radiation
        memoized_G_o[doy0][k] = 0.;
      }
    }
  }

  // Grab memoized values
  G_o[0] = memoized_G_o[doy0][0];
  G_o[1] = memoized_G_o[doy0][1];
}


/** @brief Cloud effects on incoming solar radiation by Kasten and Czeplak 1980

  Estimate global horizontal irradiation H_gh from
  clear-sky horizontal radiation H_{clearsky,h} as
    `H_gh = k_c * H_{clearsky}`
  where k_c represents the effects of cloud cover based
  on a relationship developed by Kasten and Czeplak 1980 @cite kasten1980SE.

  @param cloud_cover Fraction of sky covered by clouds [0-1].

  @return k_c [0-1]
*/
double overcast_attenuation_KastenCzeplak1980(double cloud_cover)
{
  return fmax(0., fmin(1., 1. - 0.75 * pow(cloud_cover, 3.4)));
}

/** @brief Cloud effects on incoming solar radiation by Angstrom 1924

  Estimate global horizontal irradiation H_gh from
  clear-sky horizontal radiation H_{clearsky,h} as
    `H_gh = k_c * H_{clearsky}`
  where k_c represents the effects of relative sunshine,
  e.g., limited due to cloud cover,
  based on a relationship developed by Angstrom 1924 @cite angstrom1924QJRMS.

  Note: This is not equivalent to the more widely used Angstrom-Prescott type
  models that estimate H_gh from extraterrestrial radiation H_oh.

  @param sunshine_fraction Ratio of actual to possible hours of sunshine [0-1]

  @return k_c [0-1]
*/
double overcast_attenuation_Angstrom1924(double sunshine_fraction)
{
  return fmax(0., fmin(1., 0.25 + 0.75 * sunshine_fraction));
}




/** @brief Atmospheric attenuation by clear-sky model of direct beam radiation

  Clearness index of direct beam radiation
  based on relationships developed by Majumbar et al. (1972)
  and ASCE-EWRI 2005 updated coefficients
  as cited by Allen et al. 2006 @cite allen2006AaFM

  @note Allen's empirical turbidity coefficient is here fixed at 1.

  @param P Atmospheric pressure [kPa]
  @param e_a Actual vapor pressure [kPa]
  @param int_sin_beta Daily integral of sin(beta)
    where beta is the sun angle (= solar altitude angle)

  @return Clearness index of direct beam radiation for cloudless conditions [-]
*/
double clearsky_directbeam(double P, double e_a, double int_sin_beta)
{
  double W, Kt = 1., K_b;

  /* Allen et al. 2006: "Kt is an empirical turbidity coefficient,
     0 < Kt <= 1.0 where Kt = 1.0 for clean air (typical of regions of
     agricultural and natural vegetation) and Kt = 0.5 for extremely turbid,
     dusty or polluted air"
  */

  // Equivalent depth of precipitable water in the atmosphere [mm]
  W = 2.1 + 0.14 * e_a * P; // Allen et al. 2006: eq. 18

  // Allen et al. 2006: eq. 17
  if (GT(int_sin_beta, 0.)) {
    K_b = 0.98 * exp(-0.00146 * P / (Kt * int_sin_beta) \
           - 0.075 * pow(W / int_sin_beta, 0.4));

  } else {
    K_b = 0.;
  }

  return fmax(0., fmin(1., K_b));
}

/** @brief Clearness index of diffuse radiation

  Based on relationships developed by Boes (1981) and ASCE-EWRI 2005 updated
  coefficients as cited by Allen et al. 2006 @cite allen2006AaFM

  @param K_b Clearness index of direct beam radiation
    for cloudless conditions [-]

  @return Clearness index of diffuse radiation [-]
*/
double clearnessindex_diffuse(double K_b)
{
  double K_d;

  // Allen et al. 2006: eq. 19
  if (GE(K_b, 0.15)) {
    K_d = 0.35 - 0.36 * K_b;

  } else if (GT(K_b, 0.065)) {
    K_d = 0.18 + 0.82 * K_b;

  } else {
    K_d = 0.10 + 2.08 * K_b;
  }

  return fmax(0., fmin(1., K_d));
}


/** @brief Daily global irradiation on (tilted) surface

  We use the clear-sky and separation model
  by Allen et al. 2006 @cite allen2006AaFM which is
  based on Majumdar et al. 1972 and updated by ASCE-EWRI 2005;
  additionally, we incorporate effects of cloud cover based
  on a relationship developed by Angstrom 1924 @cite angstrom1924QJRMS to
  represent all-sky conditions.

  We use the anisotropic HDKR transposition model
  (Reindl et al. 1990 @cite reindl1990SE) to
  transpose direct and diffuse radiation to a tilted surface.

  @param[in] doy Day of year [1-365].
  @param[in] lat Latitude of the site [radians].
  @param[in] elev Elevation of the site [m a.s.l.].
  @param[in] slope Slope of the site
    between 0 (horizontal) and pi / 2 (vertical) [radians].
  @param[in] aspect Surface aspect of the site [radians].
    A value of \ref SW_MISSING indicates no data, i.e., treat it as if slope = 0;
    South facing slope: aspect = 0, East = -pi / 2, West = pi / 2, North = ±pi.
  @param[in] albedo Average albedo of the surrounding ground surface
    below the inclined surface [0-1].
  @param[in] cloud_cover Fraction of sky covered by clouds [0-1].
  @param[in] e_a Actual vapor pressure [kPa]

  @param[out] H_oh Daily extraterrestrial horizontal irradiation [MJ / m2]
  @param[out] H_ot Daily extraterrestrial tilted irradiation [MJ / m2]
  @param[out] H_gh Daily global horizontal irradiation [MJ / m2]
  @return H_gt Daily global (tilted) irradiation [MJ / m2]
*/
double solar_radiation(unsigned int doy,
  double lat, double elev, double slope, double aspect,
  double albedo, double cloud_cover, double e_a,
  double *H_oh, double *H_ot, double *H_gh)
{
  double
    P,
    sun_angles[7], int_cos_theta[2], int_sin_beta[2],
    H_o[2],
    k_c,
    H_bh, H_dh, K_bh, K_dh,
    H_bt, H_dt, H_rt, K_bt, f_ia, f_i, f_B,
    H_g;


  //--- Calculate daily integration of cos(theta) and sin(beta)
  //    for horizontal and tilted surfaces
  sun_hourangles(
    doy, lat, slope, aspect,
    sun_angles,
    int_cos_theta,
    int_sin_beta
  );


  //--- Daily extraterrestrial irradiation H_o [H_oh, H_ot]
  solar_radiation_extraterrestrial(doy, int_cos_theta, H_o);
  *H_oh = H_o[0];
  *H_ot = H_o[1];


  //--- Atmospheric attenuation
  // Calculate atmospheric pressure
  P = atmospheric_pressure(elev);

  // Atmospheric attenuation: additional cloud effects
  //k_c = overcast_attenuation_KastenCzeplak1980(cloud_cover / 100.);

  // TODO: improve estimation of sunshine_fraction n/N
  // e.g., Essa and Etman 2004 (Meteorol Atmos Physics) and
  // Matuszko 2012 (Int J Climatol) suggest that n/N != 1 - C
  k_c = overcast_attenuation_Angstrom1924(1. - cloud_cover / 100.);


  //--- Separation/decomposition: separate global horizontal irradiation H_gh
  // into direct and diffuse radiation components

  // Atmospheric attenuation: clear-sky model for direct beam irradiation
  K_bh = clearsky_directbeam(P, e_a, int_sin_beta[0]);
  H_bh = K_bh * k_c * (*H_oh); // Allen et al. 2006: eq. 24 + k_c

  // Diffuse irradiation
  K_dh = clearnessindex_diffuse(K_bh);
  H_dh = K_dh * k_c * (*H_oh); // Allen et al. 2006: eq. 25 + k_c

  // Global horizontal irradiation: Allen et al. 2006: eq. 23
  *H_gh = H_bh + H_dh;


  //--- Transposition: transpose direct and diffuse radiation to tilted surface
  if (has_tilted_surface(slope, aspect)) {

    // Direct beam irradiation
    K_bt = clearsky_directbeam(P, e_a, int_sin_beta[1]);
    H_bt = K_bt * k_c * (*H_ot); // Allen et al. 2006: eq. 30 + k_c

    // Diffuse irradiation (isotropic)
    f_i = 0.75 + 0.25 * cos(slope) - slope / swPI2; // Allen et al. 2006: eq. 32


    // Diffuse irradiation (anisotropic): HDKR model (Reindl et al. 1990)
    f_B = K_bt / K_bh * (*H_ot) / (*H_oh); // Allen et al. 2006: eq. 34

    f_ia = f_i * (1. - K_bh) \
      * (1. + sqrt(K_bh / (K_bh + K_dh)) * pow(sin(slope / 2.), 3.)) \
      + f_B * K_bh; // Allen et al. 2006: eq. 33

    H_dt = f_ia * H_dh; // Allen et al. 2006: eq. 31


    // Reflected irradiation; Allen et al. 2006: eq. 36
    H_rt = albedo * (1. - f_i) * (*H_gh);


    //--- Daily global titled irradiation: Allen et al. 2006: eq. 29
    H_g = H_bt + H_dt + H_rt;

  } else {
    // Horizontal surface: no transposition; no reflected radiation
    H_g = *H_gh;
  }


  return H_g;
}



/** @brief Theoretical black-body radiation based on Stefan-Boltzmann's law

  Stefan-Boltzmann law = `sigma * Ta^4 [W / m2]`
  with Stefan-Boltzmann constant = sigma = 5.670374 * 1e-8 [W / m2 / K4]
  (http://physics.nist.gov/cgi-bin/cuu/Value?sigma).

  @param T Temperature [C]
  @return Radiation [W / m2]
*/
double blackbody_radiation(double T) {
  double tmp_K;

  tmp_K = 0.01 * (T + 273.15); // convert [C] to [Kelvin]
  return 5.670374 * squared(squared(tmp_K));
}



/** @brief Atmospheric pressure based on elevation

  Based on equation 7 (chapter 3) of Allen et al. (1998) @cite allen1998 and
  equation 3 of Allen et al. (2005) @cite ASCE2005.

  @param elev Site elevation [m above mean sea level].
  @return Atmospheric pressure [kPa]
*/
double atmospheric_pressure(double elev) {
  return 101.3 * powe((293. - 0.0065 * elev) / 293., 5.26);
}


/** @brief Psychrometric constant `gamma`

  Based on equation 8 (chapter 3) of Allen et al. (1998) @cite allen1998 and
  equation 4 of Allen et al. (2005) @cite ASCE2005.

  Prior to 2012-Oct-11, equation was based on
  Penman (1948) @cite Penman1948 : `gamma [mmHg / F] = 0.27`

  @param pressure Atmospheric pressure [kPa]

  @return Psychrometric constant [kPa / K]
*/
double psychrometric_constant(double pressure) {
  return 0.000665 * pressure;
}



/** @brief Saturation vapor pressure of water and ice and the
           slope of the saturation vapor pressure-temperature curve

  Empirical equation derived from integrating the Clausius–Clapeyron equation
  by Huang 2018 @cite huang2018JAMC.

  The slope of the svp-T curve is obtained by derivation for temperature.

  @param[in] T Temperature [C]
  @param[out] slope_svp_to_t Slope of es:T [kPa / K]

  @return Saturation vapor pressure [kPa]
*/
double svp(double T, double *slope_svp_to_t) {
  double tmp, tmp0, tmp1, tmp2, tmp3, dp, svp;

  if (T > 0) {
    // Derivation for Huang 2018: eq. 17
    tmp0 = T + 105.;
    tmp1 = pow(tmp0, 1.57);
    tmp = T + 237.1;
    tmp2 = 4924.99 / squared(tmp);
    tmp3 = exp(34.494 - 4924.99 / tmp);
    dp = 1.57 * pow(tmp0, 0.57);

  } else {
    // Derivation for Huang 2018: eq. 18
    tmp0 = T + 868.;
    tmp1 = squared(tmp0);
    tmp = T + 278.;
    tmp2 = 6545.8 / squared(tmp);
    tmp3 = exp(43.494 - 6545.8 / tmp);
    dp = 2. * tmp0;
  }

  svp = tmp3 / tmp1;
  *slope_svp_to_t = svp * tmp2 - tmp3 / squared(tmp1) * dp;
  (*slope_svp_to_t) *= 1e-3;

  return 1e-3 * svp;
}

/**
  @brief Saturation vapor pressure for calculation of actual vapor pressure

  Implements equation 7 by Allen et al. (2005) @cite ASCE2005

  @param[in] temp Daily mean, minimum, or maximum temperature or dewpoint temperature [C]

  @return Saturation vapor pressure [kPa]
*/
double svp2(double temp) {
    // Allen et al. 2005 eq 7
    return .6108 * exp((17.27 * temp) / (temp + 237.3));
}

/**
 @brief Calculate actual vapor pressure based on relative humidity and mean temperature

 Implements equation 7 and 14 by Allen et al. (2005) @cite ASCE2005

 @param hurs Daily mean relative humidity [%]
 @param meanTemp Daily mean air temperature [C]

 @return Calculated actual vapor pressure [kPa]
 */
double actualVaporPressure1(double hurs, double meanTemp) {
    // Allen et al. 2005 eqs 7 and 14
    return (hurs / 100.) * svp2(meanTemp);
}

/**
 @brief Calculate actual vapor pressure from daily minimum and maximum of
 air temperature and relative humidity

 Implements equation 7 and 11 by Allen et al. (2005) @cite ASCE2005

 @param maxHurs Daily maximum relative humidity [%]
 @param minHurs Daily minimum relative humidity [%]
 @param maxTemp Daily minimum air temperature [C]
 @param minTemp Daily maximum air temperature [C]

 @return Calculated actual vapor pressure [kPa]
 */
double actualVaporPressure2(double maxHurs, double minHurs, double maxTemp, double minTemp) {
    // Allen et al. 2005 eqs 7 and 11
    return (actualVaporPressure1(minHurs, maxTemp) + actualVaporPressure1(maxHurs, minTemp)) / 2;
}

/**
 @brief Calculate actual vapor pressure based on dew point temperature

 Implements equation 7 and 8 by Allen et al. (2005) @cite ASCE2005

 @param dewpointTemp 2m dew point temperature [C]

 @return Calculated actual vapor pressure [kPa]
 */
double actualVaporPressure3(double dewpointTemp) {
    // Allen et al. 2005 eqs 7 and 8
    return svp2(dewpointTemp);
}



/**
@brief Daily potential evapotranspiration

  Equations based on Penman 1948 @cite Penman1948.

  Note: Penman 1948 @cite Penman1948 assumes that net heat and vapor exchange
  with ground and surrounding areas is negligible over a daily time step.


  @param H_g Global horizontal/tilted irradiation [MJ / m2]
  @param avgtemp Average air temperature [C]
  @param elev Elevation of site [m asl]
  @param reflec  Albedo [-]
  @param humid Average relative humidity [%]
  @param windsp Average wind speed at 2-m above ground [m / s]
  @param cloudcov Average cloud cover [%]

  @return Potential evapotranspiration [cm / day]
*/
double petfunc(double H_g, double avgtemp, double elev,
  double reflec, double humid, double windsp, double cloudcov)
{

  double Ea, Rn, Rc, Rbb, delta, clrsky, ea, P_kPa, gamma, pet;

  /* Unit conversion factors:
   1 langley = 1 ly = 41840 J/m2 = 0.0168 evaporative-mm
    (1 [ly] / 2490 [kJ/kg heat of vaporization at about T = 10-15 C],
    see also Allen et al. (1998, ch. 1))
   1 mmHg = 101.325/760 kPa = 0.1333 kPa
   1 mile = 1609.344 m
   0 C = 273.15 K
  */
  static const double
    // [mmHg / F] = [kPa / K] * [mmHg / kPa] =
    //            = [kPa / K] * (760. / 101.325)
    convert_kPa__to__mmHg = 7.5006168,

    // [miles / day] = [m / s] * [miles / m] * [s / day] =
    //               = [m / s] * (1 / 1609.344) * 86400
    convert_m_per_s__to__miles_per_day = 53.686471,

    // [langley] = [evaporative mm] * [kJ / m2] / [heat of vaporization] =
    //           = [evaporative mm] * (41.840) / 2490
    // 2490 [kJ/kg heat of vaporization at about T = 10-15 C], see also Allen et al. (1998, ch. 1)
    // convert_ly__to__mm = 0.0168,

    // [W / m2] = [evaporative mm / day] * [kJ / s / m2] * [s / day] / [heat of vaporization] =
    //            [evaporative mm / day] * 1e-3 * 86400 / 2490
    convert_W_per_m2__to__mm_per_day = 0.0346988,

    // [MJ / m2] = [evaporative mm / day] * [1e3 kJ / m2] / [heat of vaporization] =
    //             [evaporative mm / day] * 1e3 / 2490
    convert_MJ_per_m2__to__mm_per_day = 0.4016063;


  //------ Convert input variables
  // Clear sky
  // Penman (1948): n/N = clrsky =
  //  = Ratio of actual/possible hours of sunshine =
  //  approximate with 1 - m/10 = 1 - fraction of sky covered by cloud
  clrsky = 1. - cloudcov / 100.;

  // Wind speed (2 meters above ground)
  windsp *= convert_m_per_s__to__miles_per_day;



  //------ Calculate incoming short wave radiation [mm / day]
  Rc = H_g * convert_MJ_per_m2__to__mm_per_day;


  // Calculate outgoing long wave radiation [mm / day]
  Rbb = blackbody_radiation(avgtemp);
  Rbb *= convert_W_per_m2__to__mm_per_day;


  //------ Calculate inputs to Penman's equation

  // Calculate atmospheric pressure [kPa]
  P_kPa = atmospheric_pressure(elev);

  // Calculate psychrometric constant [kPa / K]
  gamma = psychrometric_constant(P_kPa);


  // Saturation vapor pressure [mmHg]
  // and delta = slope of the svp-temperature curve [kPa / K]
  ea = svp(avgtemp, &delta); //   at air-Tave = ea
  ea *= convert_kPa__to__mmHg;

  humid *= ea / 100.; //   at dewpoint = ed = relative humidity * ea



  //------ Calculate PET using Penman (1948):

  // Evaporation rate from open water:
  // Penman (1948), eqn. 19: par1 = Ea [mm / day] = evaporation rate from
  //  open water with ea instead of es as required in eqn. 16
  Ea = .35 * (ea - humid) * (1. + .0098 * windsp);

  // Net radiant energy available at surface = net irradiance:
  // Penman (1948), eqn. 13 [mm / day]: Rn = H [mm / day]
  // This assumes that net heat and vapor exchange with ground and surrounding
  // areas is negligible over time step (Penman 1948 eqn. 9)
  Rn = (1. - reflec) * Rc - Rbb * (.56 - .092 * sqrt(humid)) * (.10 + .90 * clrsky);

  // Penman's evaporation from open water = potential evapotranspiration
  // Penman (1948), eqn. 16: E [mm / day]
  // note: eqn. 16 expects units of delta and gamma to be [mmHg / F]; however,
  //       these units cancel each other out. Thus, we use here [kPa / K]
  pet = (delta * Rn + gamma * Ea) / (delta + gamma);

  return fmax(0.1 * pet, 0.01); // PET [cm / day]
}
