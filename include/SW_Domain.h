#ifndef SWDOMAIN_H
#define SWDOMAIN_H

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*                  Local Definitions                  */
/* --------------------------------------------------- */
void SW_DOM_calc_ncStartSuid(SW_DOMAIN* SW_Domain, unsigned long suid,
                             unsigned long ncStartSuid[]);
void SW_DOM_calc_nSUIDs(SW_DOMAIN* SW_Domain);
Bool SW_DOM_CheckProgress(char* domainType, unsigned long ncStartSuid[]);
void SW_DOM_CreateProgress(SW_DOMAIN* SW_Domain);
void SW_DOM_construct(unsigned long rng_seed, SW_DOMAIN* SW_Domain);
void SW_DOM_read(SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo);
void SW_DOM_SetProgress(char* domainType, unsigned long ncStartSuid[]);
void SW_DOM_SimSet(SW_DOMAIN* SW_Domain, unsigned long userSUID,
                   LOG_INFO* LogInfo);
void SW_DOM_deepCopy(SW_DOMAIN* source, SW_DOMAIN* dest, LOG_INFO* LogInfo);

#ifdef __cplusplus
}
#endif

#endif // SWDOMAIN_H
