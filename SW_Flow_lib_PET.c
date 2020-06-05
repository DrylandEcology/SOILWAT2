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
#include "generic.h"
#include "SW_Defines.h"

#include "SW_Flow_lib_PET.h"


/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */


/* *************************************************** */
/*                Module-Level Variables               */
/* --------------------------------------------------- */

/** @brief Solar constant

  Solar constant = 1367 [W / m2], World Radiation Center (WRC)
  convert to daily irradiance:
     - 24 * 60 * 60 [s/day] convert W/m2 -> J/day/m2
     - 1e-6 convert J -> MJ
   G_sc [MJ / (m2 * day)]
*/
static const double G_sc = 118.1088;

#define has_tilted_surface(slope, aspect) ( GT( (slope), 0. ) && !missing( (aspect) ) )


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

  Hour angle values range from negative at sunrise \f$ \omega = -\omega_s \f$,
  via \f$ \omega = 0 \f$ at solar noon to positive at sunset
  \f$ \omega = \omega_s \f$.

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
  for a horizontal and a tilted surface based on equations developed
  by Allen et al. 2006 @cite allen2006AaFM.
  However, we calculate candidate hour angle values via `±acos(cos(omega))`
  instead of `asin(sin(omega))` to allow for full range of omega values.

  @param[in] doy Day of year [1-365].
  @param[in] lat Latitude of the site [radians].
  @param[in] slope Slope of the site
    between 0 (horizontal) and pi / 2 (vertical) [radians].
  @param[in] aspect Surface aspect of the site [radians].
    A value of \ref SW_MISSING indicates no data, i.e., treat it as if slope = 0;
    South facing slope: aspect = 0, East = -pi / 2, West = pi / 2, North = ±pi.

  @param[out] int_cos_theta Array of length 2 with
    daily integral during sunshine (one or two periods)
    of the cosine of the solar incidence angle
    on a horizontal (1st element) and tilted (2nd element) surface [-]

  @param[out] int_sin_beta Array of length 2 with
    daily integral during sunshine (one or two periods)
    of the sine of the sun angle (= solar altitude angle)
    above a horizontal (1st element) and tilted (2nd element) surface [-]
*/
void sun_hourangles(unsigned int doy, double lat, double slope, double aspect,
 double int_cos_theta[], double int_sin_beta[])
{
  static const double tol3 = 0.001;
  unsigned int i;
  double
    sun_angles[8],
    declin,
    a, b, c, g, h,
    f1, f2, f3, f4, f5,
    tmp, tmp1, tmp2, tmp3, X,
    omega1, omega1x, trig_omega1, omega1b,
    omega2, omega2x, trig_omega2, omega2b,
    cos_theta_sunrise, cos_theta_sunset,
    cos_theta1, cos_theta1x, cos_theta1b,
    cos_theta2, cos_theta2x, cos_theta2b;

  /*
  @param[out] sun_angles Array of size 7 returning with
    0. indicator:
       - sun never rises above horizontal surface = -2
       - sun never rises above tilted surface = -1
       - sun never sets = 0
       - sun rises and sets once for tilted surface = 1
       - sun rises and sets twice for tilted surface = 2
    1. sunrise hour angle on horizontal surface \f$ -\omega_s \f$
       (equal to the negative of the sunset hour angle on horizontal surface)
    2. first sunrise hour angle on tilted surface
    3. first sunset hour angle on tilted surface (if two periods of sunshine)
    4. second sunrise hour angle on tilted surface (if two periods of sunshine)
    5. final sunset hour angle on tilted surface
    6. sunset hour angle on horizontal surface \f$ \omega_s \f$

  */

  // Initialize in case we return early (without sunshine)
  for (i = 0; i < 7; i++) {
    sun_angles[i] = SW_MISSING;
  }

  int_cos_theta[0] = 0.;
  int_cos_theta[1] = 0.;
  int_sin_beta[0] = 0.;
  int_sin_beta[1] = 0.;

  // Calculate solar declination
  declin = solar_declination(doy);


  //------ Horizontal surfaces: sunset and sunrise angles

  if (GE(lat, 0.)) {
    // Northern hemisphere
    if (GT(lat + declin, swPI_half)) {
      // sun never sets
      sun_angles[0] = 0;
      sun_angles[6] = swPI;

    } else if (GT(lat - declin, swPI_half)) {
      // sun never rises
      sun_angles[0] = -2;
      sun_angles[1] = 0.;
      sun_angles[6] = 0.;
      return;

    } else {
      sun_angles[0] = 1;
      sun_angles[6] = sunset_hourangle(lat, declin);
    }

  } else {
    // Southern hemisphere
    if (LT(lat + declin, -swPI_half)) {
      // sun never sets
      sun_angles[0] = 0;
      sun_angles[6] = swPI;

    } else if (LT(lat - declin, -swPI_half)) {
      // sun never rises
      sun_angles[0] = -2;
      sun_angles[1] = 0.;
      sun_angles[6] = 0.;
      return;

    } else {
      sun_angles[0] = 1;
      sun_angles[6] = sunset_hourangle(lat, declin);
    }
  }

  // Sunrise hour angle on horizontal surface is
  // equal to the negative of the sunset hour angle
  sun_angles[1] = -sun_angles[6];


  // Integrate on a horizontal surface from sunrise to sunset
  g = sin(declin) * sin(lat);
  h = cos(declin) * cos(lat);


  // Integrate the sine of the sun angle (= solar altitude angle)
  // above a horizontal surface from sunrise to sunset
  tmp1 = 2. * squared(g) * sun_angles[6] \
       + 4. * g * h * sin(sun_angles[6]) \
       + squared(h) * (sun_angles[6] + 0.5 * sin(2. * sun_angles[6]));
  tmp2 = g * sun_angles[6] + h * sin(sun_angles[6]);
  int_sin_beta[0] = fmax(0., tmp1 / (2. * tmp2)); // Allen et al. 2006: eq. 26


  // Integrate the cosine of the solar incidence angle on a horizontal surface
  // from sunrise to sunset: Allen et al. 2006: eq. 35
  // standardize by pi because integral is across half day (0 to sunset)
  int_cos_theta[0] = tmp2 / swPI;



  //------ Tilted surfaces: sunset and sunrise angles
  if (has_tilted_surface(slope, aspect)) {
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
    cos_theta_sunrise = -a + b * cos(sun_angles[1]) + c * sin(sun_angles[1]);
    cos_theta_sunset = -a + b * cos(sun_angles[6]) + c * sin(sun_angles[6]);

    // Calculate candidate sunrise and sunset hour angles
    // on tilted surface (Eq. 11-13)
    tmp3 = squared(b) + squared(c);
    tmp = tmp3 - squared(a);
    tmp = sqrt(GT(tmp, 0.) ? tmp : 0.0001);
    tmp2 = b * tmp; // see Step Di (Appendix A)

/* Calculation of omega via asin(sin(omega)) as per Eq. 12-13
    tmp1 = a * c;
    trig_omega1 = (tmp1 - tmp2) / tmp3;
    trig_omega1 = fmax(-1., fmin(1., trig_omega1)); // see Step Diii (Appendix A)
    omega1 = asin(trig_omega1);

    trig_omega2 = (tmp1 + tmp2) / tmp3;
    trig_omega2 = fmax(-1., fmin(1., trig_omega2)); // see Step Diii (Appendix A)
    omega2 = asin(trig_omega2);
*/

/* Calculation of omega via acos(cos(omega))
   Problem: omega can take values in [-pi, +pi]; however,
            C99 asin() returns values in [-pi/2, +pi/2] --> we double pack
   Solution: C99 acos() returns values in [0, pi] & account for sign

   Derivation:
    Start with Eq. 10: cos(omega) = a / b - c / b * sin(omega)
    Solve for cos(omega) instead of sin(omega) as done in Eqs 11-13:
      c / b * sin(omega) = a / b - cos(omega)
      sin(omega) = a / c - b / c * cos(omega)
      cos(omega) = (a * b - c * sqrt(b^2 + c^2 - a^2)) / (b^2 + c^2)
*/
    tmp1 = a * b;
    trig_omega1 = (tmp1 + c * tmp) / tmp3;
    trig_omega1 = fmax(-1., fmin(1., trig_omega1));
    trig_omega2 = (tmp1 - c * tmp) / tmp3;
    trig_omega2 = fmax(-1., fmin(1., trig_omega2));
    omega1 = -acos(trig_omega1);
    omega2 = acos(trig_omega2);


    // Candidate sunrise and sunset hour angles on tilted surface
    cos_theta1 = -a + b * cos(omega1) + c * sin(omega1);
    cos_theta2 = -a + b * cos(omega2) + c * sin(omega2);


    // Step B. Determine the beginning integration limit representing
    // the initial incidence of the center of the solar beam on the slope
    if (LE(cos_theta_sunrise, cos_theta1) && LT(cos_theta1, tol3)) {
      // Sunrise on tilted surface occurs after sunrise on horizontal surface
      sun_angles[2] = omega1;

    } else {
      // Sunsrise occurs with the sun above the tilted surface
      omega1x = -swPI - omega1;
      cos_theta1x = -a + b * cos(omega1x) + c * sin(omega1x);

      if (GT(cos_theta1x, tol3)) {
        sun_angles[2] = sun_angles[1];

      } else if (LE(cos_theta1x, tol3) && LE(omega1x, sun_angles[1])) {
        sun_angles[2] = sun_angles[1];

      } else {
        sun_angles[2] = omega1x;
      }
    }

    if (LT(sun_angles[2], sun_angles[1])) {
      // prevent "transparent" earth
      sun_angles[2] = sun_angles[1];
    }


    // Step C. Determine the ending integration limit representing
    // the final incidence of the center of the solar beam on the slope
    if (LE(cos_theta_sunset, cos_theta2) && LT(cos_theta2, tol3)) {
      // Sunrise on tilted surface occurs before sunset on horizontal surface
      sun_angles[5] = omega2;

    } else {
      // Sunset occurs with the sun above the tilted surface
      omega2x = swPI - omega2;
      cos_theta2x = -a + b * cos(omega2x) + c * sin(omega2x);

      if (GT(cos_theta2x, tol3)) {
        sun_angles[5] = sun_angles[6];

      } else if (LE(cos_theta2x, tol3) && GE(omega2x, sun_angles[6])) {
        sun_angles[5] = sun_angles[6];

      } else {
        sun_angles[5] = omega2x;
      }
    }

    if (GT(sun_angles[5], sun_angles[6])) {
      // prevent "transparent" earth
      sun_angles[5] = sun_angles[6];
    }


    // Step D. Additional limits for numerical stability and
    // twice per day periods of sun

    if (GE(sun_angles[2], sun_angles[5])) {
      // slope is always shaded
      sun_angles[0] = -1;
      sun_angles[2] = 0.;
      sun_angles[5] = 0.;
      return;
    }


    tmp = sin(lat) * cos(declin) + cos(lat) * sin(declin);
    if (GT(sin(slope), tmp)) {
      // possibility for two periods of sunshine
      omega2b = fmin(omega1, omega2);
      omega1b = fmax(omega1, omega2);

      cos_theta1b = -a + b * cos(omega1b) + c * sin(omega1b);
      cos_theta2b = -a + b * cos(omega2b) + c * sin(omega2b);

      if (GT(fabs(cos_theta1b), tol3)) {
        omega1b = swPI - omega1b;
      }

      if (GT(fabs(cos_theta2b), tol3)) {
        omega2b = -swPI - omega2b;
      }

      if (GE(omega2b, sun_angles[2]) && LE(omega1b, sun_angles[5])) {
        // two periods of sunshine are still possible
        X = - a * (omega1b - omega2b)
            + b * (sin(omega1b) - sin(omega2b))
            - c * (cos(omega1b) - cos(omega2b));

        if (LT(X, 0.)) {
          // indeed two periods of sunshine
          sun_angles[0] = 2;
          sun_angles[3] = omega2b;
          sun_angles[4] = omega1b;
        }
      }
    }


    // Integrate from (first) sunrise to (last) sunset
    if (EQ(sun_angles[0], 2.)) {
      // Two periods of sunshine
      f1 = sin(sun_angles[3]) - sin(sun_angles[2]) \
         + sin(sun_angles[5]) - sin(sun_angles[4]);
      f2 = cos(sun_angles[3]) - cos(sun_angles[2]) \
         + cos(sun_angles[5]) - cos(sun_angles[4]);
      f3 = sun_angles[3] - sun_angles[2] + sun_angles[5] - sun_angles[4];
      f4 = sin(2. * sun_angles[3]) - sin(2. * sun_angles[2]) \
         + sin(2. * sun_angles[5]) - sin(2. * sun_angles[4]);
      f5 = squared(sin(sun_angles[3])) - squared(sin(sun_angles[2])) \
         + squared(sin(sun_angles[5])) - squared(sin(sun_angles[4]));

    } else {
      // One period of sunshine
      f1 = sin(sun_angles[5]) - sin(sun_angles[2]);
      f2 = cos(sun_angles[5]) - cos(sun_angles[2]);
      f3 = sun_angles[5] - sun_angles[2];
      f4 = sin(2. * sun_angles[5]) - sin(2. * sun_angles[2]);
      f5 = squared(sin(sun_angles[5])) - squared(sin(sun_angles[2]));
    }


   // Integrate the sine of the sun angle (= solar altitude angle)
   // above tilted surface from (first) sunrise to (last) sunset
    tmp1 = f1 * (b * g - a * h) \
         - f2 * c * g \
         + f3 * (0.5 * b * h - a * g) \
         + f4 * 0.25 * b * h \
         + f5 * 0.5 * c * h;
    tmp2 = b * f1 - c * f2 - a * f3;
    int_sin_beta[1] = fmax(0., tmp1 / tmp2); //  Allen et al. 2006: eq. 22


    // Integrate the cosine of the solar incidence angle on tilted surface
    // from (first) sunrise to (last) sunset
    // Allen et al. 2006: eqs 5 (one sunshine period) and 51 (two periods)
    // standardize by 2 * pi because integral is across full day
    int_cos_theta[1] = tmp2 / swPI2;
  }

}



/** @brief Daily extraterrestrial solar irradiation

  This function approximates the origin of the solar beam as the center of
  the solar disk.
  The function corrects daily irradiation to the amount that would be received
  on a tilted surface on Earth in the absence of an atmosphere.

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
  double di2 = 1.;
  int k;


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
      G_o[k] = G_sc * di2 * int_cos_theta[k];

    } else {
      // no radiation
      G_o[k] = 0.;
    }
  }
}


/** @brief Cloud effects on incoming solar radiation

  Estimate global horizontal irradiation H_gh from
  clear-sky horizontal radiation H_{clearsky,h} as
    `H_gh = k_c * H_{clearsky}`
  where k_c represents the effects of cloud cover based
  on a relationship developed by Kasten and Czeplak 1980 @cite kasten1980SE.

  @param cloud_cover Fraction of sky covered by clouds [0-1].

  @return k_c [0-1]
*/
double atmospheric_attenuation_cloudfactor(double cloud_cover) {
  return 1. - 0.75 * pow(cloud_cover, 3.4);
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
  double W, Kt = 1.;
  /* Allen et al. 2006: "Kt is an empirical turbidity coefficient,
     0 < Kt <= 1.0 where Kt = 1.0 for clean air (typical of regions of
     agricultural and natural vegetation) and Kt = 0.5 for extremely turbid,
     dusty or polluted air"
  */

  // Equivalent depth of precipitable water in the atmosphere [mm]
  W = 2.1 + 0.14 * e_a * P; // Allen et al. 2006: eq. 18

  // Allen et al. 2006: eq. 17
  return 0.98 * exp(-0.00146 * P / (Kt * int_sin_beta) \
         - 0.075 * pow(W / int_sin_beta, 0.4));
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

  return K_d;
}


/** @brief Daily global irradiation on (tilted) surface

  We use the clear-sky and separation model
  by Allen et al. 2006 @cite allen2006AaFM which is
  based on Majumdar et al. 1972 and updated by ASCE-EWRI 2005;
  additionally, we incorporate effects of cloud cover based
  on a relationship developed by Kasten and Czeplak 1980 @cite kasten1980SE.

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
  @param[in] rel_humidity Daily mean relative humidity [%]
  @param[in] air_temp_mean Daily mean air temperature [C]

  @param[out] H_oh Daily extraterrestrial horizontal irradiation [MJ / m2]
  @param[out] H_gh Daily global horizontal irradiation [MJ / m2]
  @return H_gt Daily global (tilted) irradiation [MJ / m2]
*/
double solar_radiation(unsigned int doy,
  double lat, double elev, double slope, double aspect,
  double albedo, double cloud_cover, double rel_humidity, double air_temp_mean,
  double *H_oh, double *H_gh)
{
  double
    P, e_a,
    int_cos_theta[2], int_sin_beta[2],
    H_o[2],
    k_c,
    H_bh, H_dh, K_bh, K_dh,
    H_bt, H_dt, H_rt, K_bt, f_ia, f_i, f_B,
    H_g;

  // Calculate atmospheric pressure
  P = atmospheric_pressure(elev);

  // Actual vapor pressure [kPa] estimated from daily mean air temperature and
  // mean monthly relative humidity at air-Tave
  // Allen et al. 2005: eqs 7 and 14
  e_a = rel_humidity / 100. *
    0.6108 * exp((273.15 + air_temp_mean) / (air_temp_mean + 237.3));


  //--- Calculate daily integration of cos(theta) and sin(beta)
  //    for horizontal and tilted surfaces
  sun_hourangles(doy, lat, slope, aspect, int_cos_theta, int_sin_beta);


  //--- Daily extraterrestrial irradiation H_o [H_oh, H_ot]
  solar_radiation_extraterrestrial(doy, int_cos_theta, H_o);
  *H_oh = H_o[0];

  //--- Separation/decomposition: separate global horizontal irradiation H_gh
  // into direct and diffuse radiation components

  // Atmospheric attenuation: additional cloud effects
  k_c = atmospheric_attenuation_cloudfactor(cloud_cover / 100.);

  // Atmospheric attenuation: clear-sky model for direct beam irradiation
  K_bh = k_c * clearsky_directbeam(P, e_a, int_sin_beta[0]);
  H_bh = K_bh * (*H_oh); // Allen et al. 2006: eq. 24

  // Diffuse irradiation
  K_dh = clearnessindex_diffuse(K_bh);
  H_dh = K_dh * (*H_oh); // Allen et al. 2006: eq. 25

  // Global horizontal irradiation: Allen et al. 2006: eq. 23
  *H_gh = H_bh + H_dh;


  //--- Transposition: transpose direct and diffuse radiation to tilted surface
  if (has_tilted_surface(slope, aspect)) {

    // Direct beam irradiation
    K_bt = k_c * clearsky_directbeam(P, e_a, int_sin_beta[1]);
    H_bt = K_bt * H_o[1]; // Allen et al. 2006: eq. 30


    // Diffuse irradiation (isotropic)
    f_i = 0.75 + 0.25 * cos(slope) - slope / swPI2; // Allen et al. 2006: eq. 32


    // Diffuse irradiation (anisotropic): HDKR model (Reindl et al. 1990)
    f_B = K_bt / K_bh * H_o[1] / (*H_oh); // Allen et al. 2006: eq. 34

    f_ia = f_i * (1. - K_bh) \
      * (1 + sqrt(K_bh / (K_bh + K_dh)) * pow(sin(slope / 2.), 3.)) \
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



/** @brief Slope of the saturation vapor pressure-temperature curve

  Based on equation 13 (chapter 3) of Allen et al. (1998) @cite allen1998 and
  equation 5 of Allen et al. (2005) @cite ASCE2005.

  Prior to 2012-Oct-11, equation `vapor * 3010.21 / (kelvin * kelvin)`
  was of unknown origin.
  However, results are very similar to the current implementation.

  @param es_at_tmean Saturation vapor pressure at average temperature [kPa]
  @param tmean Average temperature for the day [&deg;C]

  @return slope of es:T at T=Ta [kPa / K]
*/
double slope_svp_to_t(double es_at_tmean, double tmean) {
  return 4098. * es_at_tmean / squared(tmean + 237.3);
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


/** @brief Saturation vapor pressure of water.

  Equations based on Hess 1959. @cite Hess1961

  @author SLC

  @param  temp Average temperature for the day (&deg;C).

  @return svapor Saturation vapor pressure (mm of Hg/&deg;F).
*/
double svapor(double temp) {
	double par1, par2;

	par1 = 1. / (temp + 273.);
	par2 = log(6.11) + 5418.38 * (.00366 - par1); /*drs: par2 = ln(es [mbar]) = ln(es(at T = 273.15K) = 6.11 [mbar]) + (mean molecular mass of water vapor) * (latent heat of vaporization) / (specific gas constant) * (1/(273.15 [K]) - 1/(air temperature [K])) */

	return (exp(par2) * .75);
}


/**
@brief Daily potential evapotranspiration

  Equations based on Penman 1948 @cite Penman1948, ASCE 2000 @cite ASCE2000,
  Bowen 1926 @cite Bowen1926, Brunt 1939 @cite Brunt1939,
  Kopp et al. 2011 @cite Kopp2011, and Sellers 1965 @cite Sellers1965 .

  Note: Penman 1948 @cite Penman1948 assumes that net heat and vapor exchange
  with ground and surrounding areas is negligible over time step.


  @param H_g Daily global horizontal/tilted irradiation [MJ / m2]
  @param avgtemp Average air temperature for the day (&deg;C).
  @param elev	Elevation of site (m).
  @param reflec  Unitless measurement of albedo.
  @param humid Average relative humidity for the month (%).
  @param windsp Average wind speed for the month at 2-m above ground (m/s).
  @param cloudcov Average cloud cover for the month (%).

  @return Potential evapotranspiration [cm / day].
*/
double petfunc(double H_g, double avgtemp, double elev,
  double reflec, double humid, double windsp, double cloudcov)
{
/***********************************************************************
HISTORY:
4/30/92  (SLC)
10/11/2012	(drs)	annotated all equations;
replaced unknown equation for solar declination with one by Spencer (1971);
replaced unknown equation for 'Slope of the Saturation Vapor Pressure-Temperature Curve' = arads with one provided by Allen et al. (1998) and (2005);
replaced constant psychrometric constant (0.27 [mmHg/F]) as function of pressure, and pressure as function of elevation of site (Allen et al. (1998) and (2005));
windspeed data is in [m/s] and not in code-required [miles/h] -> fixed conversation factor so that code now requires [m/s];
replaced conversion addend from C to K (previously, 273) with 273.15;
updated solar constant from S = 2.0 [ly/min] to S = 1.952 [ly/min] (Kopp et al. 2011) in the equation to calculate solrad;
replaced unknown numerical factor of 0.201 in black-body long wave radiation to sigma x conversion-factor = 0.196728 [mm/day/K4];
--> further update suggestions:
- add Seller's factor '(mean(d)/d)^2' with (mean(d) = mean distance and d = instantaneous distance of the earth from the sun) to shortwave calculations
11/06/2012	(clk)	allowed slope and aspect to be used in the calculation for solar radiation;
if slope is 0, will still use old equation,
else will sum up Seller (1965), page 35, eqn. 3.15 from sunrise to sunset.

SOURCES:
Allen RG, Pereira LS, Raes D, Smith M (1998) In Crop evapotranspiration - Guidelines for computing crop water requirements. FAO - Food and Agriculture Organizations of the United Nations, Rome.
Allen RG, Walter IA, Elliott R, Howell T, Itenfisu D, Jensen M (2005) In The ASCE standardized reference evapotranspiration equation, pp. 59. ASCE-EWRI Task Committee Report.
Bowen IS (1926) The Ratio of Heat Losses by Conduction and by Evaporation from any Water Surface. Physical Review, 27, 779.
Brunt D (1939) Physical and dynamical meteorology. Cambridge University Press.
Kopp G, Lean JL (2011) A new, lower value of total solar irradiance: Evidence and climate significance. Geophysical Research Letters, 38, L01706.
Merva GE (1975) Physioengineering principles. Avi Pub. Co., Westport, Conn., ix, 353 p. pp.
Penman HL (1948) Natural evaporation form open water, bare soil and grass. Proceedings of the Royal Society of London. Series A, Mathematical and Physical Sciences, 193, 120-145.
Sellers WD (1965) Physical climatology. University of Chicago Press, Chicago, USA.

LOCAL VARIABLES:
solrad - solar radiation (ly/day)
declin - solar declination (radians)
ahou   - sunset hour angle
azmth  - azimuth angle of the sun
azmthSlope - azimuth angle of the slope
rslope - slope of the site (radians)
hou    - hour angle
shwave - short wave solar radiation (mm/day)
kelvin - average air temperature [K]
Delta  - 'Slope of the Saturation Vapor Pressure-Temperature Curve' [mmHg/F]
clrsky - relative amount of clear sky
fhumid - saturation vapor pressure at dewpoint [mmHg]
ftemp  - theoretical black-body radiation [mm/day]
par1,par2 - parameters in computation of pet.

***********************************************************************/

  double Ea, Rn, Rc, Rbb, Delta, clrsky, vapor, P, gamma, pet;

  /* Unit conversion factors:
   1 langley = 1 ly = 41840 J/m2 = 0.0168 evaporative-mm
    (1 [ly] / 2490 [kJ/kg heat of vaporization at about T = 10-15 C],
    see also Allen et al. (1998, ch. 1))
   1 mmHg = 101.325/760 kPa = 0.1333 kPa
   1 mile = 1609.344 m
   0 C = 273.15 K
  */
  static const double
    // [mmHg / F] = [kPa / K] * [mmHg / kPa] * [K / F] =
    //            = [kPa / K] * (760. / 101.325) * (5. / 9.)
    convert_kPa_per_K__to__mmHg_per_F = 4.1670093484,

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



  //------ Calculate radiation
  Rc = H_g * convert_MJ_per_m2__to__mm_per_day;


  // Calculate long wave radiation:
  Rbb = blackbody_radiation(avgtemp);
  Rbb *= convert_W_per_m2__to__mm_per_day;


  //------ Calculate inputs to Penman's equation

  // Calculate atmospheric pressure
  P = atmospheric_pressure(elev);

  // Calculate psychrometric constant
  gamma = psychrometric_constant(P);
  gamma *= convert_kPa_per_K__to__mmHg_per_F;

  // Saturation vapor pressure at air-Tave [mmHg] ea = vapor
  vapor = svapor(avgtemp);

  // Slope of the Saturation Vapor Pressure-Temperature Curve:
  Delta = slope_svp_to_t(vapor, avgtemp);
  Delta *= 5. / 9.;
  // TODO: why not: Delta *= convert_kPa_per_K__to__mmHg_per_F;

  // Saturation vapor pressure at dewpoint [mmHg] = ed = relative humidity * ea
  humid *= vapor / 100.;



  //------ Calculate PET using Penman (1948):

  // Evaporation rate from open water:
  // Penman (1948), eqn. 19: par1 = Ea [mm / day] = evaporation rate from
  //  open water with ea instead of es as required in eqn. 16
  Ea = .35 * (vapor - humid) * (1. + .0098 * windsp);

  // Net radiant energy available at surface = net irradiance:
  // Penman (1948), eqn. 13 [mm / day]: Rn = H [mm / day]
  // This assumes that net heat and vapor exchange with ground and surrounding
  // areas is negligible over time step (Penman 1948 eqn. 9)
  Rn = (1. - reflec) * Rc - Rbb * (.56 - .092 * sqrt(humid)) * (.10 + .90 * clrsky);

  // Penman's evaporation from open water = potential evapotranspiration
  // Penman (1948), eqn. 16: E [mm / day]
  pet = (Delta * Rn + gamma * Ea) / (Delta + gamma);

  return fmax(0.1 * pet, 0.01); // PET [cm / day]
}
