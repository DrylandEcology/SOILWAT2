#ifndef SWDOMAIN_H
#define SWDOMAIN_H

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*                  Local Definitions                  */
/* --------------------------------------------------- */

void SW_DOM_read(PATH_INFO* PathInfo, SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo);
void SW_DOM_setModelTime(SW_MODEL *SW_Model, SW_DOMAIN *SW_Domain);

#ifdef __cplusplus
}
#endif

#endif // SWDOMAIN_H
