#ifndef _HASHMAP_H_INCLUDED
#define _HASHMAP_H_INCLUDED

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

/* HashMap definitions */
enum {
  RBT_RED,
  RBT_BLACK
};

typedef struct _tagHASHMAPPAIR HASHMAPPAIR, * LPHASHMAPPAIR;
typedef struct _tagHASHMAPNODE HASHMAPNODE, * LPHASHMAPNODE;
typedef struct _tagHASHMAP HASHMAP, * LPHASHMAP;

struct _tagHASHMAPPAIR {
  void* pKey;
  void* pValue;
  size_t keySize;
  size_t valueSize;
};

struct _tagHASHMAPNODE {
  HASHMAPPAIR pair;
  LPHASHMAPNODE pParent;
  LPHASHMAPNODE pLeft;
  LPHASHMAPNODE pRight;
  int color;
};

struct _tagHASHMAP {
  LPHASHMAPNODE pRoot;
  size_t keySize;
  int (*compare)(const void* key1, size_t key1Size, const void* key2, size_t key2Size);
};

LPHASHMAPNODE HashMap_CreateNode(HASHMAPPAIR pair);
void HashMap_LeftRotate(LPHASHMAP pHashMap, LPHASHMAPNODE pX);
void HashMap_RightRotate(LPHASHMAP pHashMap, LPHASHMAPNODE pY);
void HashMap_InsertFixup(LPHASHMAP pHashMap, LPHASHMAPNODE pZ);
void HashMap_InsertNode(LPHASHMAP pHashMap, LPHASHMAPNODE pZ);
void InitializeHashMap(LPHASHMAP pHashMap, size_t keySize, int (*compare)(const void*, size_t, const void*, size_t));
LPHASHMAPNODE HashMap_SearchNode(LPHASHMAP pHashMap, const void* pKey, size_t keySize);
void HashMap_Insert(LPHASHMAP pHashMap, const void* pKey, void* pValue, size_t valueSize);
void* HashMap_Get(LPHASHMAP pHashMap, const void* pKey);
void HashMap_FreeMemory(LPHASHMAPNODE pNode);
void HashMap_Cleanup(LPHASHMAP pHashMap);
int compareInt(const void* key1, size_t key1Size, const void* key2, size_t key2Size);

#endif  /* _HASHMAP_H_INCLUDED */
