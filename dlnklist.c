#include "precomp.h"

#include "dlnklist.h"

#ifdef UNIT_TESTING
extern void* _test_malloc(const size_t size, const char* file, const int line);
 
#define malloc(size) _test_malloc(size, __FILE__, __LINE__)

extern void* _test_calloc(const size_t num, const size_t size, const char* file,
  const int line);

#define calloc(num, size) _test_calloc(num, size, __FILE__, __LINE__)
#endif

void DoubleLinkList_Init(LPDOUBLELINKLIST pDoubleLinkList, DOUBLELINKLISTSORTFUNC pfnSort)
{
  pDoubleLinkList->pBegin = NULL;
  pDoubleLinkList->pEnd = NULL;
  pDoubleLinkList->pHead = NULL;
  pDoubleLinkList->pfnSort = pfnSort;
}

LPDOUBLELINKLISTNODE DoubleLinkList_CreateNode(void)
{
  LPDOUBLELINKLISTNODE pNode = (LPDOUBLELINKLISTNODE) malloc(sizeof(DOUBLELINKLISTNODE));
  if (pNode)
  {
    pNode->pNext = NULL;
    pNode->pPrev = NULL;
    pNode->pValue = NULL;
    pNode->valueSize = 0;

    return pNode;
  }

  return NULL;
}

/*
 * DoubleLinkList_AppendBack
 *
 * Insert an element at the beginning of double linked list
 * `pValue` any data passed by void pointer to be copied
 * `valueSize` size of the data to be copied
 * `bSeek` - set the pHead pointer to the inserted node
 * */
void DoubleLinkList_AppendBack(LPDOUBLELINKLIST pDoubleLinkList, const void* pValue, size_t valueSize, BOOL bSeek)
{
  LPDOUBLELINKLISTNODE pNode = DoubleLinkList_CreateNode();

  assert(pNode);
  pNode->pValue = malloc(valueSize);
  if (pNode->pValue) {
    pNode->valueSize = valueSize;

    memcpy(pNode->pValue, pValue, valueSize);

    if (!pDoubleLinkList->pEnd) {
      pDoubleLinkList->pEnd = pNode;
    }

    if (bSeek) {
      pDoubleLinkList->pHead = pNode;
    }

    if (pDoubleLinkList->pBegin) {
      pDoubleLinkList->pBegin->pPrev = pNode;
      pNode->pNext = pDoubleLinkList->pBegin;
    }

    pDoubleLinkList->pBegin = pNode;
  }
}

/*
 * DoubleLinkList_AppendFront
 *
 * Insert an element at the end of double linked list
 * `pValue` any data passed by void pointer to be copied
 * `valueSize` size of the data to be copied
 * `bSeek` - set the pHead pointer to the inserted node
 * */
void DoubleLinkList_AppendFront(LPDOUBLELINKLIST pDoubleLinkList, const void* pValue, size_t valueSize, BOOL bSeek)
{
  LPDOUBLELINKLISTNODE pNode = DoubleLinkList_CreateNode();
  if (pNode) {
    pNode->valueSize = valueSize;

    pNode->pValue = malloc(valueSize);
    if (pNode->pValue) {
      memcpy(pNode->pValue, pValue, valueSize);

      if (!pDoubleLinkList->pBegin) {
        pDoubleLinkList->pBegin = pNode;
      }

      if (bSeek) {
        pDoubleLinkList->pHead = pNode;
      }

      if (pDoubleLinkList->pEnd) {
        pDoubleLinkList->pEnd->pNext = pNode;
        pNode->pPrev = pDoubleLinkList->pEnd;
      }

      pDoubleLinkList->pEnd = pNode;
    }
  }
}

/*
 * DoubleLinkList_Sort
 *
 * Sort the provided double linked list by an algorithm provided in pfnSort
 * callback function
 *
 * Note: if pfnSort is not provided, the function will fail
 * */
void DoubleLinkList_Sort(LPDOUBLELINKLIST pDoubleLinkList) {
  if (!pDoubleLinkList->pfnSort) {
    fprintf(stderr, "Error: Sorting function not provided.\n");
    return;
  }

  /* List is empty, nothing to sort */
  if (!pDoubleLinkList->pBegin) {
    return;
  }

  for (LPDOUBLELINKLISTNODE pNode1 = pDoubleLinkList->pBegin; pNode1; pNode1 = pNode1->pNext) {
    for (LPDOUBLELINKLISTNODE pNode2 = pNode1->pNext; pNode2; pNode2 = pNode2->pNext) {
      if (pDoubleLinkList->pfnSort(pNode1->pValue, pNode1->valueSize, pNode2->pValue, pNode2->valueSize) < 0) {
        /* Swap data pointers */
        void* temp = pNode1->pValue;
        pNode1->pValue = pNode2->pValue;
        pNode2->pValue = temp;
      }
    }
  }
}

/*
 * Function to free memory allocated for the list
 */
void DoubleLinkList_Free(LPDOUBLELINKLIST pDoubleLinkList) {
  LPDOUBLELINKLISTNODE pCurrent = pDoubleLinkList->pBegin;
  while (pCurrent) {
    LPDOUBLELINKLISTNODE pNext = pCurrent->pNext;
    free(pCurrent->pValue);
    free(pCurrent);
    pCurrent = pNext;
  }

  /* Reset list pointers */
  pDoubleLinkList->pBegin = pDoubleLinkList->pEnd = pDoubleLinkList->pHead = NULL;
}
