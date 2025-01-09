/* myMemory.h -- declarations for routines in myMemory.c */

/* see also memblock.h and myMemory.c */

#ifndef MYMEMORY_H
#define MYMEMORY_H

#include "include/SW_datastructs.h" // for LOG_INFO
#include <stdio.h>                  // for size_t


#ifdef __cplusplus
extern "C" {
#endif


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
char *Str_Dup(const char *s, LOG_INFO *LogInfo);

void *Mem_Malloc(size_t size, const char *funcname, LOG_INFO *LogInfo);

void *Mem_Calloc(
    size_t nobjs, size_t size, const char *funcname, LOG_INFO *LogInfo
);

void *Mem_ReAlloc(void *block, size_t sizeNew, LOG_INFO *LogInfo);

void Mem_Set(void *block, byte c, size_t n);

void Mem_Copy(void *dest, const void *src, size_t n);

void *sw_memccpy_custom(
    void *__restrict dest, void *__restrict src, int c, size_t n
);

Bool sw_memccpy_inc(
    void **__restrict charPtr, void *__restrict str, int c, size_t *n
);

void sw_memccpy_report(
    Bool forOutput, Bool fullBuffer, char *endPtr, LOG_INFO *LogInfo
);

#ifdef __cplusplus
}
#endif

#endif
