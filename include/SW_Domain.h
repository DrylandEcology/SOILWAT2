#ifndef SWDOMAIN_H
#define SWDOMAIN_H

#include "include/SW_datastructs.h"

/* =================================================== */
/*                  Local Definitions                  */
/* --------------------------------------------------- */

#define NUM_DOM_IN_KEYS 8 // Number of possible keys within `domain.in`
#define KEY_NOT_FOUND -1 // A key within `domain.in` is not a valid one

void SW_DOM_read(SW_MODEL* SW_Model, char *InFiles[],
    SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo);
int domain_inkey_to_id(char *key);

#endif // SWDOMAIN_H
