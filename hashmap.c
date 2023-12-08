#include "hashmap.h"

#ifdef UNIT_TESTING
extern void* _test_malloc(const size_t size, const char* file, const int line);

#define malloc(size) _test_malloc(size, __FILE__, __LINE__)

extern void* _test_calloc(const size_t num, const size_t size, const char* file,
  const int line);

#define calloc(num, size) _test_calloc(num, size, __FILE__, __LINE__)
#endif

LPHASHMAPNODE HashMap_CreateNode(HASHMAPPAIR pair)
{
  LPHASHMAPNODE pNewNode = (LPHASHMAPNODE)malloc(sizeof(HASHMAPNODE));

  pNewNode->pair = pair;
  pNewNode->pParent = NULL;
  pNewNode->pLeft = NULL;
  pNewNode->pRight = NULL;
  pNewNode->color = RBT_RED;  /* New nodes are always red */

  return pNewNode;
}

void HashMap_LeftRotate(LPHASHMAP pHashMap, LPHASHMAPNODE pX)
{
  LPHASHMAPNODE pY = pX->pRight;
  pX->pRight = pY->pLeft;

  if (pY->pLeft) {
    pY->pLeft->pParent = pX;
  }

  pY->pParent = pX->pParent;

  if (!pX->pParent) {
    pHashMap->pRoot = pY;
  }
  else if (pX == pX->pParent->pLeft) {
    pX->pParent->pLeft = pY;
  }
  else {
    pX->pParent->pRight = pY;
  }

  pY->pLeft = pX;
  pX->pParent = pY;
}

void HashMap_RightRotate(LPHASHMAP pHashMap, LPHASHMAPNODE pY)
{
  LPHASHMAPNODE pX = pY->pLeft;
  pY->pLeft = pX->pRight;

  if (pX->pRight) {
    pX->pRight->pParent = pY;
  }

  pX->pParent = pY->pParent;

  if (!pY->pParent) {
    pHashMap->pRoot = pX;
  }
  else if (pY == pY->pParent->pLeft) {
    pY->pParent->pLeft = pX;
  }
  else {
    pY->pParent->pRight = pX;
  }

  pX->pRight = pY;
  pY->pParent = pX;
}

void HashMap_InsertFixup(LPHASHMAP pHashMap, LPHASHMAPNODE pZ)
{
  while (pZ->pParent && pZ->pParent->color == RBT_RED) {
    if (pZ->pParent == pZ->pParent->pParent->pLeft) {

      LPHASHMAPNODE pY = pZ->pParent->pParent->pRight;
      if (pY && pY->color == RBT_RED) {
        pZ->pParent->color = RBT_BLACK;
        pY->color = RBT_BLACK;
        pZ->pParent->pParent->color = RBT_RED;
        pZ = pZ->pParent->pParent;
      }
      else {
        if (pZ == pZ->pParent->pRight) {
          pZ = pZ->pParent;
          HashMap_LeftRotate(pHashMap, pZ);
        }
        pZ->pParent->color = RBT_BLACK;
        pZ->pParent->pParent->color = RBT_RED;
        HashMap_RightRotate(pHashMap, pZ->pParent->pParent);
      }
    }
    else {
      LPHASHMAPNODE pY = pZ->pParent->pParent->pLeft;
      if (pY && pY->color == RBT_RED) {
        pZ->pParent->color = RBT_BLACK;
        pY->color = RBT_BLACK;
        pZ->pParent->pParent->color = RBT_RED;
        pZ = pZ->pParent->pParent;
      }
      else {
        if (pZ == pZ->pParent->pLeft) {
          pZ = pZ->pParent;
          HashMap_RightRotate(pHashMap, pZ);
        }
        pZ->pParent->color = RBT_BLACK;
        pZ->pParent->pParent->color = RBT_RED;
        HashMap_LeftRotate(pHashMap, pZ->pParent->pParent);
      }
    }
  }
  pHashMap->pRoot->color = RBT_BLACK;
}

void HashMap_InsertNode(LPHASHMAP pHashMap, LPHASHMAPNODE pZ)
{
  LPHASHMAPNODE pY = NULL;
  LPHASHMAPNODE pX = pHashMap->pRoot;

  while (pX) {
    pY = pX;
    if (pHashMap->compare(pZ->pair.pKey, pZ->pair.keySize, pX->pair.pKey, pX->pair.keySize) < 0) {
      pX = pX->pLeft;
    }
    else {
      pX = pX->pRight;
    }
  }

  pZ->pParent = pY;
  if (!pY) {
    pHashMap->pRoot = pZ;
  }
  else if (pHashMap->compare(pZ->pair.pKey, pZ->pair.keySize, pY->pair.pKey, pY->pair.keySize) < 0) {
    pY->pLeft = pZ;
  }
  else {
    pY->pRight = pZ;
  }

  HashMap_InsertFixup(pHashMap, pZ);
}

void InitializeHashMap(LPHASHMAP pHashMap, size_t keySize, int (*compare)(const void*, size_t, const void*, size_t)) {
  pHashMap->pRoot = NULL;
  pHashMap->keySize = keySize;
  pHashMap->compare = compare;
}

LPHASHMAPNODE HashMap_SearchNode(LPHASHMAP pHashMap, const void* pKey, size_t keySize)
{
  LPHASHMAPNODE pCurrent = pHashMap->pRoot;

  while (pCurrent) {
    int cmp = pHashMap->compare(pKey, keySize, pCurrent->pair.pKey, pCurrent->pair.keySize);
    if (cmp == 0) {
      return pCurrent;
    }
    else if (cmp < 0) {
      pCurrent = pCurrent->pLeft;
    }
    else {
      pCurrent = pCurrent->pRight;
    }
  }

  return NULL;
}

void HashMap_Insert(LPHASHMAP pHashMap, const void* pKey, void* pValue, size_t valueSize)
{
  LPHASHMAPNODE pExistingNode = HashMap_SearchNode(pHashMap, pKey, pHashMap->keySize);

  /* If the key already exists, update the value */
  if (pExistingNode) {
    pExistingNode->pair.pValue = realloc(pExistingNode->pair.pValue, valueSize);
    memcpy(pExistingNode->pair.pValue, pValue, valueSize);
    pExistingNode->pair.valueSize = valueSize;
  }
  else {
    HASHMAPPAIR pair;
    pair.pKey = malloc(pHashMap->keySize);
    memcpy(pair.pKey, pKey, pHashMap->keySize);
    pair.pValue = malloc(valueSize);
    memcpy(pair.pValue, pValue, valueSize);
    pair.keySize = pHashMap->keySize;
    pair.valueSize = valueSize;

    LPHASHMAPNODE pNewNode = HashMap_CreateNode(pair);
    HashMap_InsertNode(pHashMap, pNewNode);
  }
}

/* Get the value associated with a key from the Red-Black Tree hashmap */
void* HashMap_Get(LPHASHMAP pHashMap, const void* pKey)
{
  LPHASHMAPNODE pNode = HashMap_SearchNode(pHashMap, pKey, pHashMap->keySize);
  return pNode ? pNode->pair.pValue : NULL;
}

void HashMap_FreeMemory(LPHASHMAPNODE pNode) {
  if (pNode) {
    HashMap_FreeMemory(pNode->pLeft);
    HashMap_FreeMemory(pNode->pRight);
    free(pNode->pair.pKey);
    free(pNode->pair.pValue);
    free(pNode);
  }
}

void HashMap_Cleanup(LPHASHMAP pHashMap)
{
  HashMap_FreeMemory(pHashMap->pRoot);
  pHashMap->pRoot = NULL;
}

int compareInt(const void* key1, size_t key1Size, const void* key2, size_t key2Size)
{
  if (key1Size != sizeof(int) || key2Size != sizeof(int)) {
    /* Error: Incorrect key size */
    return -1;
  }

  int intKey1 = *((const int*)key1);
  int intKey2 = *((const int*)key2);

  return intKey1 - intKey2;
}
