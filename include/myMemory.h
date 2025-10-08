/* myMemory.h -- declarations for routines in myMemory.c */

/* see also memblock.h and myMemory.c */

#ifndef MYMEMORY_H
#define MYMEMORY_H

#include "include/SW_datastructs.h" // for LOG_INFO
#include <stdio.h>                  // for size_t
#include <string.h>                 // for memccpy

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
    void **__restrict charPtr,
    char *__restrict endPtr,
    void *__restrict str,
    int c,
    size_t *n
);

/* Memory copying via `sw_memccpy()` and SOILWAT2's custom function */
#if (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L) || \
    (defined(__STDC__) && defined(__STDC_VERSION__) &&          \
     __STDC_VERSION__ >= 202311L)
#define sw_memccpy memccpy
#else
#define sw_memccpy sw_memccpy_custom
#endif

void reportFullBuffer(int errmode, LOG_INFO *LogInfo);

#ifdef __cplusplus
}
#endif

#endif
