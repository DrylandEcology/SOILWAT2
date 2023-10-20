#ifndef SWDOMAIN_H
#define SWDOMAIN_H

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*                  Local Definitions                  */
/* --------------------------------------------------- */
int* SW_DOM_calc_ncStartSuid(SW_DOMAIN* SW_Domain, int suid);
void SW_DOM_calc_nSUIDs(SW_DOMAIN* SW_Domain);
Bool SW_DOM_CheckProgress(char* domainType, int* ncStartSuid);
void SW_DOM_CreateProgress(SW_DOMAIN* SW_Domain);
void SW_DOM_read(char *InFiles[], SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo);
void SW_DOM_SetProgress(char* domainType, int* ncStartSuid);
void SW_DOM_SimSet(SW_DOMAIN* SW_Domain, int userSUID, int nSUIDs,
                   int* startSimSet, int* endSimSet, LOG_INFO* LogInfo);

#ifdef __cplusplus
}
#endif

#endif // SWDOMAIN_H
