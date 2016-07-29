#include "lcl.h"

#pragma GCC diagnostic push  // require GCC 4.6
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-W"

long hash31(int64_t a, int64_t b, int64_t x)
{

  int64_t result;
  long lresult;

  // return a hash of x using a and b mod (2^31 - 1)
// may need to do another mod afterwards, or drop high bits
// depending on d, number of bad guys
// 2^31 - 1 = 2147483647

  //  result = ((int64_t) a)*((int64_t) x)+((int64_t) b);
  result=(a * x) + b;
  result = ((result >> HL) + result) & MOD;
  lresult=(long) result;

  return(lresult);
}

/********************************************************************
Implementation of Lazy Lossy Counting algorithm to Find Frequent Items
Based on the paper of Manku and Motwani, 2002
And Metwally, Agrwawal and El Abbadi, 2005
Implementation by G. Cormode 2002, 2003, 2005
This implements the space saving algorithm, which 
guarantees 1/epsilon space. 
This implementation uses a heap to track which is the current smallest count

Original Code: 2002-11
This version: 2002,2003,2005,2008

This work is licensed under the Creative Commons
Attribution-NonCommercial License. To view a copy of this license,
visit http://creativecommons.org/licenses/by-nc/1.0/ or send a letter
to Creative Commons, 559 Nathan Abbott Way, Stanford, California
94305, USA.
*********************************************************************/

LCL_type * LCL_Init(float fPhi)
{
	int i;
	int k = 1 + (int) 1.0/fPhi;

	LCL_type *result = (LCL_type *) calloc(1,sizeof(LCL_type));
	// needs to be odd so that the heap always has either both children or 
	// no children present in the data structure

	result->size = (1 + k) | 1; // ensure that size is odd
	result->hashsize = LCL_HASHMULT*result->size;
	result->hashtable=(LCLCounter **) calloc(result->hashsize,sizeof(LCLCounter*));
	result->counters=(LCLCounter*) calloc(1+result->size,sizeof(LCLCounter));
	// indexed from 1, so add 1

	result->hasha=151261303;
	result->hashb=6722461; // hard coded constants for the hash table,
	//should really generate these randomly
	result->n=(LCLweight_t) 0;

	for (i=1; i<=result->size;i++)
	{
		result->counters[i].next=NULL;
		result->counters[i].prev=NULL;
		result->counters[i].item=LCL_NULLITEM;
		// initialize items and counters to zero
	}
	result->root=&result->counters[1]; // put in a pointer to the top of the heap
	return(result);
}

void LCL_Destroy(LCL_type * lcl)
{
	free(lcl->hashtable);
	free(lcl->counters);
	free(lcl);
}

void LCL_RebuildHash(LCL_type * lcl)
{
	// rebuild the hash table and linked list pointers based on current
	// contents of the counters array
	int i;
	LCLCounter * pt;

	for (i=0; i<lcl->hashsize;i++)
		lcl->hashtable[i]=0;
	// first, reset the hash table
	for (i=1; i<=lcl->size;i++) {
		lcl->counters[i].next=NULL;
		lcl->counters[i].prev=NULL;
	}
	// empty out the linked list
	for (i=1; i<=lcl->size;i++) { // for each item in the data structure
		pt=&lcl->counters[i];
		pt->next=lcl->hashtable[lcl->counters[i].hash];
		if (pt->next)
			pt->next->prev=pt;
		lcl->hashtable[lcl->counters[i].hash]=pt;
	}
}

static void Heapify(LCL_type * lcl, int ptr)
{ // restore the heap condition in case it has been violated
	LCLCounter tmp;
	LCLCounter * cpt, *minchild;
	int mc;

	while(1)
	{
		if ((ptr<<1) + 1>lcl->size) break;
		// if the current node has no children

		cpt=&lcl->counters[ptr]; // create a current pointer
		mc=(ptr<<1)+
			((lcl->counters[ptr<<1].count<lcl->counters[(ptr<<1)+1].count)? 0 : 1);
		minchild=&lcl->counters[mc];
		// compute which child is the lesser of the two

		if (cpt->count < minchild->count) break;
		// if the parent is less than the smallest child, we can stop

		tmp=*cpt;
		*cpt=*minchild;
		*minchild=tmp;
		// else, swap the parent and child in the heap

		if (cpt->hash==minchild->hash)
			// test if the hash value of a parent is the same as the 
			// hash value of its child
		{ 
			// swap the prev and next pointers back. 
			// if the two items are in the same linked list
			// this avoids some nasty buggy behaviour
			minchild->prev=cpt->prev;
			cpt->prev=tmp.prev;
			minchild->next=cpt->next;
			cpt->next=tmp.next;
		} else { // ensure that the pointers in the linked list are correct
			// check: hashtable has correct pointer (if prev ==0)
			if (!cpt->prev) { // if there is no previous pointer
				if (cpt->item!=LCL_NULLITEM)
					lcl->hashtable[cpt->hash]=cpt; // put in pointer from hashtable
			} else
				cpt->prev->next=cpt;
			if (cpt->next) 
				cpt->next->prev=cpt; // place in linked list

			if (!minchild->prev) // also fix up the child
				lcl->hashtable[minchild->hash]=minchild; 
			else
				minchild->prev->next=minchild; 
			if (minchild->next)
				minchild->next->prev=minchild;
		}
		ptr=mc;
		// continue on with the heapify from the child position
	} 
}

static LCLCounter * LCL_FindItem(LCL_type * lcl, LCLitem_t item)
{ // find a particular item in the date structure and return a pointer to it
	LCLCounter * hashptr;
	int hashval;

	hashval=(int) hash31(lcl->hasha, lcl->hashb,item) % lcl->hashsize;
	hashptr=lcl->hashtable[hashval];
	// compute the hash value of the item, and begin to look for it in 
	// the hash table

	while (hashptr) {
		if (hashptr->item==item)
			break;
		else hashptr=hashptr->next;
	}
	return hashptr;
	// returns NULL if we do not find the item
}

void LCL_Update(LCL_type * lcl, LCLitem_t item, LCLweight_t value)
{
	int hashval;
	LCLCounter * hashptr;
	// find whether new item is already stored, if so store it and add one
	// update heap property if necessary

	lcl->n+=value;
	lcl->counters->item=0; // mark data structure as 'dirty'

	hashval=(int) hash31(lcl->hasha, lcl->hashb,item) % lcl->hashsize;
	hashptr=lcl->hashtable[hashval];
	// compute the hash value of the item, and begin to look for it in 
	// the hash table

	while (hashptr) {
		if (hashptr->item==item) {
			hashptr->count+=value; // increment the count of the item
			Heapify(lcl,hashptr-lcl->counters); // and fix up the heap
			return;
		}
		else hashptr=hashptr->next;
	}
	// if control reaches here, then we have failed to find the item
	// so, overwrite smallest heap item and reheapify if necessary
	// fix up linked list from hashtable
	if (!lcl->root->prev) // if it is first in its list
		lcl->hashtable[lcl->root->hash]=lcl->root->next;
	else
		lcl->root->prev->next=lcl->root->next;
	if (lcl->root->next) // if it is not last in the list
		lcl->root->next->prev=lcl->root->prev;
	// update the hash table appropriately to remove the old item

	// slot new item into hashtable
	hashptr=lcl->hashtable[hashval];
	lcl->root->next=hashptr;
	if (hashptr)
		hashptr->prev=lcl->root;
	lcl->hashtable[hashval]=lcl->root;
	// we overwrite the smallest item stored, so we look in the root
	lcl->root->prev=NULL;
	lcl->root->item=item;
	lcl->root->hash=hashval;
	lcl->root->delta=lcl->root->count;
	// update the implicit lower bound on the items frequency
	//  value+=lcl->root->delta;
	// update the upper bound on the items frequency
	lcl->root->count=value+lcl->root->delta;
	Heapify(lcl,1); // restore heap property if needed
	// return value;
}

int LCL_Size(LCL_type * lcl)
{ // return the size of the data structure in bytes
	return sizeof(LCL_type) + (lcl->hashsize * sizeof(int)) + 
		(lcl->size*sizeof(LCLCounter));
}

LCLweight_t LCL_PointEst(LCL_type * lcl, LCLitem_t item)
{ // estimate the count of a particular item
	LCLCounter * i;
	i=LCL_FindItem(lcl,item);
	if (i)
		return(i->count);
	else
		return 0;
}

LCLweight_t LCL_PointErr(LCL_type * lcl, LCLitem_t item)
{ // estimate the worst case error in the estimate of a particular item
	LCLCounter * i;
	i=LCL_FindItem(lcl,item);
	if (i)
		return(i->delta);
	else
		return lcl->root->delta;
}

int LCL_cmp( const void * a, const void * b) {
	LCLCounter * x = (LCLCounter*) a;
	LCLCounter * y = (LCLCounter*) b;
	if (x->count<y->count) return -1;
	else if (x->count>y->count) return 1;
	else return 0;
}

void LCL_Output(LCL_type * lcl) { // prepare for output
	if (lcl->counters->item==0) {
		qsort(&lcl->counters[1],lcl->size,sizeof(LCLCounter),LCL_cmp);
		LCL_RebuildHash(lcl);
		lcl->counters->item=1;
	}
}

/*
std::map<uint32_t, uint32_t> LCL_Output(LCL_type * lcl, int thresh)
{
	std::map<uint32_t, uint32_t> res;

	for (int i=1;i<=lcl->size;++i)
	{
		if (lcl->counters[i].count>=thresh)
			res.insert(std::pair<uint32_t, uint32_t>(lcl->counters[i].item, lcl->counters[i].count));
	}

	return res;
}
*/

static void LCL_CheckHash(LCL_type * lcl, int item, int hash)
{ // debugging routine to validate the hash table
	int i;
	LCLCounter * hashptr, * prev;

	for (i=0; i<lcl->hashsize;i++)
	{
		prev=NULL;
		hashptr=lcl->hashtable[i];
		while (hashptr) {
			if (hashptr->hash!=i)
			{
				printf("\n Hash violation! hash = %d, should be %d \n", 
					hashptr->hash,i);
				printf("after inserting item %d with hash %d\n", item, hash);
			}
			if (hashptr->prev!=prev)
			{
				printf("\n Previous violation! prev = %d, should be %d\n",
					(int)hashptr->prev, (int)prev);
				printf("after inserting item %d with hash %d\n",item, hash);
				exit(EXIT_FAILURE);
			}
			prev=hashptr;
			hashptr=hashptr->next;
		}
	}
}

void LCL_ShowHash(LCL_type * lcl)
{ // debugging routine to show the hashtable
	int i;
	LCLCounter * hashptr;

	for (i=0; i<lcl->hashsize;i++)
	{
		printf("%d:",i);
		hashptr=lcl->hashtable[i];
		while (hashptr) {
			printf(" %d [h(%u) = %d, prev = %d] ---> ",(int) hashptr,
				(unsigned int) hashptr->item,
				hashptr->hash,
				(int) hashptr->prev);
			hashptr=hashptr->next;
		}
		printf(" *** \n");
	}
}

void LCL_ShowHeap(LCL_type * lcl)
{ // debugging routine to show the heap
	int i, j;

	j=1;
	for (i=1; i<=lcl->size; i++)
	{
		printf("%d ",(int) lcl->counters[i].count);
		if (i==j) 
		{ 
			printf("\n");
			j=2*j+1;
		}
	}
	printf("\n\n");
}
