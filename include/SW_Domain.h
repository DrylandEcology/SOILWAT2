#ifndef SWDOMAIN_H
#define SWDOMAIN_H

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*                  Local Definitions                  */
/* --------------------------------------------------- */
int* SW_DOM_calc_ncStartSuid(SW_DOMAIN* SW_Domain, unsigned long suid);
void SW_DOM_calc_nSUIDs(SW_DOMAIN* SW_Domain);
Bool SW_DOM_CheckProgress(char* domainType, unsigned long ncStartSuid[]);
void SW_DOM_CreateProgress(SW_DOMAIN* SW_Domain);
void SW_DOM_read(char *InFiles[], SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo);
void SW_DOM_SetProgress(char* domainType, unsigned long ncStartSuid[]);
void SW_DOM_SimSet(SW_DOMAIN* SW_Domain, unsigned long userSUID,
                   unsigned long nSUIDs, unsigned long* startSimSet,
                   unsigned long* endSimSet, LOG_INFO* LogInfo);

#ifdef __cplusplus
}
#endif

#endif // SWDOMAIN_H
