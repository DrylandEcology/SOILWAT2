/* myMemory.c  - collection of routines to handle memory
 * management beyond malloc() and free().
 *
 * Some of the code comes from "Writing Solid Code" by
 * Steve Maguire, 1993 (Microsoft Press), and some comes
 * from "A Memory Controller" by Robert A. Moeser, (Dr.
 * Dobbs Journal, 1990, v164).
 *
 * The basic idea is to add bookkeeping information from
 * allocation requests to provide feedback regarding typical
 * memory management problems such as
 - using (dereferencing) pointers to memory that has already
 been freed,
 - dereferencing pointers before/without allocating their
 memory,
 - dereferencing pointers outside the allocated memory block.

 * Some of the code is mine.  I added a define called DEBUG_MEM
 * to distinguish this and other types of debugging, such as
 * other algorithms, etc.  However, the other code makes heavy
 * use of the standard assert macro which relies on the DEBUG
 * define.  So in some places you might see DEBUG or DEBUG_MEM
 * depending on the context.
 * IF 'DEBUG_MEM' IS DEFINED, MAKE SURE 'DEBUG' IS DEFINED ALSO.

 * - CWBennett 7/17/01 */


/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/myMemory.h"       // for Mem_Calloc, Mem_Copy, Mem_Malloc
#include "include/filefuncs.h"      // for LogError
#include "include/generic.h"        // for LOGERROR, byte, isnull
#include "include/SW_datastructs.h" // for LOG_INFO
#include <stdlib.h>                 // for free, malloc, realloc
#include <string.h>                 // for strlen, memcpy, memset, strcpy

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/*****************************************************/
char *Str_Dup(const char *s, LOG_INFO *LogInfo) {
    /*-------------------------------------------
     Duplicate a string s by allocating memory for
     it, copying s to the new location and
     returning a pointer to the new string.

     Provides error handling by failing.

     cwb - 9/13/01

     -------------------------------------------*/

    char *p;

    p = (char *) Mem_Malloc(strlen(s) + 1, "Str_Dup()", LogInfo);
    if (LogInfo->stopRun) {
        return NULL; // Exit function prematurely due to error
    }

    strcpy(p, s);

    return p;
}

/*****************************************************/
void *Mem_Malloc(size_t size, const char *funcname, LOG_INFO *LogInfo) {
    /*-------------------------------------------
     Provide a generic malloc that tests for failure.

     cwb - 6/27/00
     5/19/01 - added debugging code for memory checking
     - added Mem_Set() for default behavior
     7/23/01 - added code from Macguire to install
     memory logging functions.  Modified
     the Mem_Set call to fit his suggestions.
     Changed the names to Mem_* to be more
     consistent with other modules.
     -------------------------------------------*/

    void *p = NULL;

    if (LogInfo->stopRun) {
        LogError(
            LogInfo,
            LOGERROR,
            "Mem_Malloc() by %s() called with existing error.",
            funcname
        );
        return p; // Exit function prematurely due to error
    }

    p = malloc(size);

    if (p == NULL) {
        LogError(LogInfo, LOGERROR, "Out of memory in %s()", funcname);
    }

    return p;
}

/*****************************************************/
void *Mem_Calloc(
    size_t nobjs, size_t size, const char *funcname, LOG_INFO *LogInfo
) {
    /*-------------------------------------------
     Duplicates the behavior of calloc() similar to
     Mem_Malloc.

     cwb - 8/10/01
     -------------------------------------------*/

    void *p;

    p = Mem_Malloc(size * nobjs, funcname, LogInfo);
    if (LogInfo->stopRun) {
        return NULL; // Exit function prematurely due to error
    }

    Mem_Set(p, 0, size * nobjs);

    return p;
}

/*****************************************************/
/**
@brief Wrapper for realloc()

Recognized errors include
    - \p sizeNew is zero
    - realloc() fails to allocate memory.

@param[in,out] block pointer to the memory to be reallocated.
@param[in] sizeNew new size of the array in bytes.
@param[out] LogInfo Holds information on warnings and errors.

@return On success, a pointer to the beginning of newly allocated memory;
on failure, a null pointer (and freed original pointer \p block).
*/
void *Mem_ReAlloc(void *block, size_t sizeNew, LOG_INFO *LogInfo) {

    if (sizeNew == 0) {
        free(block);
        LogError(
            LogInfo, LOGERROR, "Mem_ReAlloc() failed due to new_size = 0."
        );
        return NULL;
    }

    void *res = realloc(block, sizeNew);

    if (isnull(res)) {
        free(block);
        LogError(LogInfo, LOGERROR, "Mem_ReAlloc() failed to allocate.");
        return NULL;
    }

    return res;
}

/*****************************************************/
void Mem_Set(void *block, byte c, size_t n) {
    /*-------------------------------------------
     Provide a wrapper for memset() that facilitates debugging.

     cwb - 5/21/2001
     7/23/01  - added Macguire's code.
     -------------------------------------------*/

    memset(block, (int) c, n);
}

/*****************************************************/
void Mem_Copy(void *dest, const void *src, size_t n) {
    /*-------------------------------------------
     Provide a wrapper for memcpy() that facilitates debugging.

     cwb - 7/23/01  - added Macguire's code.
     -------------------------------------------*/

    memcpy(dest, src, n);
}

/* ===============  end of block from gen_funcs.c ----------------- */
/* ================ see also the end of this file ------------------ */
