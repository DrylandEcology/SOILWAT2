/* myMemory.h -- declarations for routines in myMemory.c */

/* see also memblock.h and myMemory.c */

#ifndef MYMEMORY_H
#define MYMEMORY_H

#include <memory.h>
#include "include/generic.h"


#ifdef DEBUG_MEM
#include "include/memblock.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
char *Str_Dup(const char *s); /* return pointer to malloc'ed dup of s */
void *Mem_Malloc(size_t size, const char *funcname);
void *Mem_Calloc(size_t nobjs, size_t size, const char *funcname);
void *Mem_ReAlloc(void *block, size_t sizeNew);
void Mem_Free(void *block);
void Mem_Set(void *block, byte c, size_t n);
void Mem_Copy(void *dest, const void *src, size_t n);


#ifdef __cplusplus
}
#endif

#endif
