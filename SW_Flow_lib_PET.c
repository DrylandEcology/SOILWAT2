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



/** @brief Calculate solar declination

    Formula is from on Spencer 1971 @cite Spencer1971.
    Errors are reported to be ±0.0007 radians (0.04 degrees);
    except near equinoxes in leap years when errors can be twice as large.

    @param doy Represents the day of the year 1-365.

    @return Solar declination in [radians].
*/
double solar_declination(unsigned int doy)
{
  /* Notes: equation used previous to Oct/11/2012 (from unknown source):
      declin = .401426 *sin(6.283185 *(doy -77.) /365.);
  */

  double dayAngle, declin;

  // Spencer (1971): dayAngle = day angle [radians]
  dayAngle = swPI2 * (doy - 1.) / 365.;

  // Spencer (1971): declin = solar declination [radians]
  declin = 0.006918 \
    - 0.399912 * cos(dayAngle) \
    + 0.070257 * sin(dayAngle) \
    - 0.006758 * cos(2. * dayAngle) \
    + 0.000907 * sin(2. * dayAngle) \
    - 0.002697 * cos(3. * dayAngle) \
    + 0.001480 * sin(3. * dayAngle);

  return declin;
}



/** @brief Calculate sunset/sunrise hour angle on a horizontal surface

    Hour angle values range from negative at sunrise \f$omega = -omega_s\f$,
    via \f$omega = 0\f$ at solar noon to positive at sunset
    \f$omega = -omega_s\f$.

    Equation based on Duffie & Beckman 2013 @cite duffie2013.

    @param rlat Latitude of the site [radians].
    @param declin Solar declination [radians].

    @return Sunset (or -sunrise) hour angle \f$omega_s\f$ [radians] on a
      horizontal surface.
*/
double sunset_hourangle(double rlat, double declin)
{
  // based on Duffie & Beckman 2013: eq. 1.6.10
  return acos(fmax(-1.0, fmin(1.0, -tan(rlat) * tan(declin))));
}




}


static double _solar_radiation_sloped(
  double rlat, double slope, double aspect,
  double ahou, double declin
) {
  double
    hou, azmth, solrad, stepSize, azmthSlope, rslope,
    cosZ, sinZ, cosA, sinA;

  /* step size is calculated by the difference in our limits of integrations,
     for hou, using 0 to ahou, divided by some resolution.
     The best resolution size seems to be around 24
  */
  stepSize = (ahou / 24.);
  // convert aspect and slope from degrees to radians:
  azmthSlope = (aspect - 180.) * deg_to_rad;
  rslope = slope * deg_to_rad;


  // pre-calculate values used in for-loop that are independent of `hou`
  const double
    sin_lat = sin(rlat),
    cos_lat = cos(rlat),
    sin_declin = sin(declin),
    cos_declin = cos(declin),
    sin_slope = sin(rslope),
    cos_slope = cos(rslope);
  const double
    sins_lat_declin = sin_lat * sin_declin,
    coss_lat_declin = cos_lat * cos_declin;


  // initialize solrad
  solrad = 0.;

  // Sellers (1965), page 35, eqn. 3.15: Qs [langlay] = solrad
  //   sum instantaneous solar radiation on a sloped surface over the
  //   period of sunrise to sunset, h=-ahou to h=ahou
  for (hou = -ahou; hou <= ahou; hou += stepSize)
  {
    // Z = zenith angle of the sun, for current hour angle
    cosZ = sins_lat_declin + coss_lat_declin * cos(hou);
    sinZ = sqrt(1. - (cosZ * cosZ));

    // determine A = azimuth angle of the sun, for current hour angle
    cosA = (sin_lat * cosZ - sin_declin) / (cos_lat * sinZ);
    sinA = (cos_declin * sin(hou)) / sinZ;
    azmth = atan2(sinA, cosA);

    // Sum instantaneous solar radiation on a sloped surface:
    solrad += stepSize *
      (cosZ * cos_slope + sinZ * sin_slope * cos(azmth - azmthSlope));
  }

  // TODO: I believe division by 2 is incorrect; it was mistakenly introduced
  // because `solar_radiation_TOA` was dividing by `2 * pi` instead of `pi`
  return solrad / 2.;
}

#ifdef SWDEBUG
  // since `_solar_radiation_sloped` is static we cannot do unit tests unless we set it up
  // as an externed function pointer
  double (*test_solar_radiation_sloped)(RealD, RealD, RealD, RealD, RealD) = &_solar_radiation_sloped;
#endif


/** @brief Daily total solar radiation on top of the atmosphere
    (extraterrestrial)

    Equations based on Sellers 1965 @cite Sellers1965.

    @param ahou Sunset hour angle [hour].
    @param declin Solar declination [radians].
    @param rlat Latitude of the site [radians].
    @param slope Slope of the site [degrees].
    @param aspect Aspect of the site [degrees].
      A value of -1 indicates no data, ie., treat it as if slope = 0;
      North = 0 deg, East = 90 deg, South = 180 deg, W = 270.

    @return Solar radiation Qs [langlay / day]
*/
double solar_radiation_TOA(
  double rlat, double slope, double aspect,
  double ahou, double declin
) {

  double solrad;

  if (!ZRO(slope) && !EQ(aspect, -1.)) {
    // account for slope-aspect effects on solar radiation
    // TODO: this should be calculated on Earth's surface not on top of atmosphere
    solrad = _solar_radiation_sloped(rlat, slope, aspect, ahou, declin);

  } else
  {
    // no slope
    // Sellers (1965), page 16, eqn. 3.7: Qs [langlay/day] = solrad =
    //   daily total solar radiation incident on a horizontal surface at the
    //   top of the atmosphere
    /* Note: factor '(mean(d)/d)^2' of Seller's equation is missing here
             with mean(d) = mean distance and
             d = instantaneous distance of the earth from the sun)
    */
    solrad = ahou * sin(rlat) * sin(declin) + cos(rlat) * cos(declin) * sin(ahou);
  }

  /* Notes:
    917. = S * 1440/pi
      with S = solar constant = 2.0 [langlay/min] (Sellers (1965), page 11)
      and with 1440 = min/day;
    however, solar constant S (Kopp et al. 2011) = 1.952 [langley/min] =
    = 1361 [W/m2] <> Seller's value of S = 2.0 [langlay/min] = 1440 [W/m2]
    => instead of factor 917 (pre-Oct 11, 2012), it should be 895
  */
  return 1440. / swPI * 1.952 * solrad;
}


/** Calculate incoming solar (short wave) radiation available at Earth's surface

  Penman (1948) @cite Penman1948 implements the Angstrom-Prescott relationship
  in equation 12 that estimates atmospheric transmittance,
  i.e., atmospheric attenuation by absorption and scattering,
  from an empirical relationship with relative sunshine hours `a + b * n/N`
  where `a` can be interpreted as minimum atmospheric transmittance and
  `a + b` as its maximum.
  Coefficients were estimated from monthly values over the period 1931-1940
  for Rothamsted, UK.

  @param Rs Extraterrestrial (incoming) solar (short wave) radiation
    on top of atmosphere. Also called Angot radiation.
  @param sunshine_fraction Ratio of actual to possible hours of sunshine [0-1]
  @return Rc [same units as Rs].
*/
double solar_radiation_surface(double Rs, double sunshine_fraction) {
  return Rs * (.18 + .55 * sunshine_fraction);
}

/** Theoretical black-body radiation based on Stefan-Boltzmann's law

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



/** @brief Slope of the Saturation Vapor Pressure-Temperature Curve

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
    @return atmospheric pressure [kPa]
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



/**
@brief Calculate the potential evapotranspiration rate.

  Equations based on Penman 1948 @cite Penman1948, ASCE 2000 @cite ASCE2000,
  Bowen 1926 @cite Bowen1926, Brunt 1939 @cite Brunt1939,
  Kopp et al. 2011 @cite Kopp2011, and Sellers 1965 @cite Sellers1965 .

  Note: Penman 1948 @cite Penman1948 assumes that net heat and vapor exchange
  with ground and surrounding areas is negligible over time step.


@param doy Represents the day of the year 1-365.
@param avgtemp Average air temperature for the day (&deg;C).
@param rlat	Latitude of the site (Radians).
@param elev	Elevation of site (m).
@param slope	Slope of the site (degrees).
@param aspect	Aspect of the site (degrees).
  A value of -1 indicates no data, ie., treat it as if slope = 0
@param reflec  Unitless measurement of albedo.
@param humid Average relative humidity for the month (%).
@param windsp Average wind speed for the month at 2-m above ground (m/s).
@param cloudcov Average cloud cover for the month (%).
@param transcoeff Transmission coefficient for the month [0-1].

@return Potential evapotranspiration [cm / day].

*/

double petfunc(unsigned int doy, double avgtemp, double rlat, double elev,
  double slope, double aspect, double reflec, double humid, double windsp,
  double cloudcov, double transcoeff)
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

  double declin, ahou, Ea, Rn, Rs, Rc, Rbb, Delta, clrsky, vapor, P, gamma, pet;

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
    convert_ly__to__mm = 0.0168,

    // [W / m2] = [evaporative mm / day] * [kJ / s / m2] * [s / day] / [heat of vaporization] =
    //            [evaporative mm / day] * 1e-3 * 86400 / 2490
    convert_W_per_m2__to__mm_per_day = 0.0346988;


  //------ Convert input variables
  // Clear sky
  // Penman (1948): n/N = clrsky =
  //  = Ratio of actual/possible hours of sunshine =
  //  approximate with 1 - m/10 = 1 - fraction of sky covered by cloud
  clrsky = 1. - cloudcov / 100.;

  // Wind speed (2 meters above ground)
  windsp *= convert_m_per_s__to__miles_per_day;



  //------ Calculate radiation terms
  // Calculate solar declination
  declin = solar_declination(doy);


  // Calculate H = half-day length = ahou = sunset hour angle
  ahou = sunset_hourangle(rlat, declin);


  // Calculate incoming solar (short wave) radiation on top of atmosphere
  Rs = solar_radiation_TOA(rlat, slope, aspect, ahou, declin);
  Rs *= convert_ly__to__mm;


  // Calculate incoming solar (short wave) radiation available at Earth's surface
  Rc = solar_radiation_surface(Rs, clrsky);
  Rc *= transcoeff; // TODO: this is probably superfluous because `solar_radiation_surface` already accounts for transmissivity


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

/**
@brief Calculates the saturation vapor pressure of water.

Equations based on Hess 1959. @cite Hess1961

@author SLC

@param  temp Average temperature for the day (&deg;C).

@return svapor Saturation vapor pressure (mm of Hg/&deg;F).

*/
double svapor(double temp) {
	/*********************************************************************
	 HISTORY:
	 4/30/92  (SLC)

	 Hess SL (1961) Introduction to theoretical meteorology. Holt, New York.

	 *********************************************************************/
	double par1, par2;

	par1 = 1. / (temp + 273.);
	par2 = log(6.11) + 5418.38 * (.00366 - par1); /*drs: par2 = ln(es [mbar]) = ln(es(at T = 273.15K) = 6.11 [mbar]) + (mean molecular mass of water vapor) * (latent heat of vaporization) / (specific gas constant) * (1/(273.15 [K]) - 1/(air temperature [K])) */

	return (exp(par2) * .75);
}
