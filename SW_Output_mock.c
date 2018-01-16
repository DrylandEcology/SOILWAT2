// Source file: mock of Output.c
// NOTE: all function content is for mock purposes only and to reduce compile warnings

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "generic.h"
#include "filefuncs.h"
#include "myMemory.h"
#include "Times.h"

#include "SW_Carbon.h"
#include "SW_Defines.h"
#include "SW_Files.h"
#include "SW_Model.h"
#include "SW_Site.h"
#include "SW_SoilWater.h"
#include "SW_Times.h"
#include "SW_Output.h"
#include "SW_Weather.h"
#include "SW_VegEstab.h"
#include "SW_VegProd.h"

// Global Variables
extern SW_SITE SW_Site;
extern SW_SOILWAT SW_Soilwat;
extern SW_MODEL SW_Model;
extern SW_WEATHER SW_Weather;
extern SW_VEGPROD SW_VegProd;
extern SW_VEGESTAB SW_VegEstab;
extern Bool EchoInits;
extern SW_CARBON SW_Carbon;

#define OUTSTRLEN 3000 /* max output string length: in get_transp: 4*every soil layer with 14 chars */

SW_OUTPUT SW_Output[SW_OUTNKEYS]; /* declared here, externed elsewhere */



/* =================================================== */
/*             Public Function Definitions             */
/* --------------------------------------------------- */
void SW_OUT_construct(void)
{}

void SW_OUT_new_year(void)
{}

void SW_OUT_read(void)
{}

void SW_OUT_close_files(void)
{}

void _collect_values(void)
{}

void SW_OUT_flush(void)
{
  _collect_values();
}

void SW_OUT_sum_today(ObjType otyp)
{
  ObjType x = otyp;
  if (x == eF) {}
}

void SW_OUT_write_today(void)
{}

static void get_none(void)
{}

static void get_outstrleader(TimeInt pd)
{
  TimeInt x = pd;
  if (x == 1) {}
}

static void get_co2effects(void)
{}

static void get_estab(void)
{}

static void get_temp(void)
{}

static void get_precip(void)
{}

static void get_vwcBulk(void)
{}

static void get_vwcMatric(void)
{}

static void get_swcBulk(void)
{}

static void get_swpMatric(void)
{}

static void get_swaBulk(void)
{}

static void get_swaMatric(void)
{}

static void get_surfaceWater(void)
{}

static void get_runoff(void)
{}

static void get_transp(void)
{}

static void get_evapSoil(void)
{}

static void get_evapSurface(void)
{}

static void get_interception(void)
{}

static void get_soilinf(void)
{}

static void get_lyrdrain(void)
{}

static void get_hydred(void)
{}

static void get_aet(void)
{}

static void get_pet(void)
{}

static void get_wetdays(void)
{}

static void get_snowpack(void)
{}

static void get_deepswc(void)
{}

static void get_soiltemp(void)
{}

static void sumof_vpd(SW_VEGPROD *v, SW_VEGPROD_OUTPUTS *s, OutKey k)
{
  OutKey x = k;
  if ((int)x == 1) {}

  if (EQ(0., v->bare_cov.fCover)) {}
  if (EQ(0., s->grass.biomass)) {}
}

static void sumof_ves(SW_VEGESTAB *v, SW_VEGESTAB_OUTPUTS *s, OutKey k)
{
  if ((int)k == 1) {}
  if (0 == v->count) {}
  if (0 == s->days) {}
}


static void sumof_wth(SW_WEATHER *v, SW_WEATHER_OUTPUTS *s, OutKey k)
{
  OutKey x = k;
  if ((int)x == 1) {}

  if (EQ(0., v->pct_snowdrift)) {}
  if (EQ(0., s->temp_max)) {}
}


static void sumof_swc(SW_SOILWAT *v, SW_SOILWAT_OUTPUTS *s, OutKey k)
{
  OutKey x = k;
  if ((int)x == 1) {}

  if (EQ(0., v->snowdepth)) {}
  if (EQ(0., s->snowdepth)) {}
}


static void average_for(ObjType otyp, OutPeriod pd)
{
  SW_OUT_sum_today(otyp);
  get_outstrleader(pd);
}

static void collect_sums(ObjType otyp, OutPeriod op)
{
  SW_OUT_sum_today(otyp);
  get_outstrleader(op);
}

void _echo_outputs(void)
{
  get_none();
  get_estab();
  get_temp();
  get_precip();
  get_vwcBulk();
  get_vwcMatric();
  get_swcBulk();
  get_swpMatric();
  get_swaBulk();
  get_swaMatric();
  get_surfaceWater();
  get_runoff();
  get_transp();
  get_evapSoil();
  get_evapSurface();
  get_interception();
  get_soilinf();
  get_lyrdrain();
  get_hydred();
  get_aet();
  get_pet();
  get_wetdays();
  get_snowpack();
  get_deepswc();
  get_soiltemp();
  get_co2effects();

  OutKey k = eSW_NoKey;
  SW_VEGPROD *vveg = NULL;
  SW_VEGPROD_OUTPUTS *sveg = NULL;
  SW_VEGESTAB *vestab = NULL;
  SW_VEGESTAB_OUTPUTS *sestab = NULL;
  SW_WEATHER *vweath = NULL;
  SW_WEATHER_OUTPUTS *sweath = NULL;
  SW_SOILWAT *vswc = NULL;
  SW_SOILWAT_OUTPUTS *sswc = NULL;

  sumof_vpd(vveg, sveg, k);
  sumof_ves(vestab, sestab, k);
  sumof_wth(vweath, sweath, k);
  sumof_swc(vswc, sswc, k);

  ObjType otyp = eF;
  OutPeriod pd = eSW_Year;

  average_for(otyp, pd);
  collect_sums(otyp, pd);
}
