// space saving algorithm with heap

#ifndef _SPACE_SAVING_HEAP_H
#define _SPACE_SAVING_HEAP_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#define MOD 2147483647
#define HL 31

// lclazy.h -- header file for Lazy Lossy Counting
// see Manku & Motwani, VLDB 2002 for details
// implementation by Graham Cormode, 2002,2003, 2005

/////////////////////////////////////////////////////////
#define LCLweight_t int
//#define LCL_SIZE 101 // size of k, for the summary
// if not defined, then it is dynamically allocated based on user parameter
////////////////////////////////////////////////////////

#define LCLitem_t uint32_t

typedef struct lclcounter_t LCLCounter;

struct lclcounter_t
{
  LCLitem_t item; // item identifier
  int hash; // its hash value
  LCLweight_t count; // (upper bound on) count for the item
  LCLweight_t delta; // max possible error in count for the value
  LCLCounter *prev, *next; // pointers in doubly linked list for hashtable
}; // 32 bytes

#define LCL_HASHMULT 3  // how big to make the hashtable of elements:
  // multiply 1/eps by this amount
  // about 3 seems to work well

#ifdef LCL_SIZE
#define LCL_SPACE (LCL_HASHMULT*LCL_SIZE)
#endif

typedef struct LCL_type
{
  LCLweight_t n;
  int hasha, hashb, hashsize;
  int size;
  LCLCounter *root;
#ifdef LCL_SIZE
  LCLCounter counters[LCL_SIZE+1]; // index from 1
  LCLCounter *hashtable[LCL_SPACE]; // array of pointers to items in 'counters'
  // 24 + LCL_SIZE*(32 + LCL_HASHMULT*4) + 8
            // = 24 + 102*(32+12)*4 = 4504
            // call it 4520 for luck...
#else
  LCLCounter *counters;
  LCLCounter ** hashtable; // array of pointers to items in 'counters'
#endif
} LCL_type;

LCL_type * LCL_Init(float fPhi);
void LCL_Destroy(LCL_type *);
void LCL_Update(LCL_type *, LCLitem_t, int);
int LCL_Size(LCL_type *);
int LCL_PointEst(LCL_type *, LCLitem_t);
int LCL_PointErr(LCL_type *, LCLitem_t);
//std::map<uint32_t, uint32_t> LCL_Output(LCL_type *,int);

void LCL_ShowHash(LCL_type * lcl);
void LCL_ShowHeap(LCL_type * lcl);

#endif /* _SPACE_SAVING_HEAP_H */
