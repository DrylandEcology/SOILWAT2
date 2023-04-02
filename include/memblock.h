/* memblock.h - header file for the memory logging data structures in
 * "Writing Solid Code" by Steve Maguire, 1993 (Microsoft Press).
 */

#ifndef MEMBLOCK_H
#define MEMBLOCK_H

#include <stdlib.h>
#include <assert.h>

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define bGarbage 0xCC
/*typedef unsigned char byte*/

/* ------------------------------------------------------------------

flag fCreateBlockInfo(byte *pbNew, size_t sizeNew);
void FreeBlockInfo(byte *pbToFree);
void UpdateBlockInfo(byte *pbOld, byte *pbNew, size_t sizeNew);
size_t sizeofBlock(byte *pb);

void ClearMemoryRefs(void);
void NoteMemoryRef(void *pv);
void CheckMemoryRefs(void);
flag fValidPointer(void *pv, size_t size);

/* --  --  --  --  --  --  --  --  --  --  --  --  --   --  --  --
 * The functions in myMemory.c must compare arbitrary pointers,
 * an operation that the ANSI standard does not guarantee to be
 * portable.
 *
 * The macros below isolate the pointer comparisons needed in
 * this file.  The implementations assume "flat" pointers, for
 * which straightforward comparisons will always work.  The
 * definitions below will *not* work for some common 80x86
 * memory models.
 */

#define fPtrLess(pLeft, pRight)    ((pLeft) <  (pRight))
#define fPtrGrtr(pLeft, pRight)    ((pLeft) >  (pRight))
#define fPtrEqual(pLeft, pRight)   ((pLeft) == (pRight))
#define fPtrLessEq(pLeft, pRight)  ((pLeft) <= (pRight))
#define fPtrGrtrEq(pLeft, pRight)  ((pLeft) >= (pRight))


#ifdef __cplusplus
}
#endif

#endif
