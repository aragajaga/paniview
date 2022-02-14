#include "precomp.h"

#include "dlnklist.h"

#ifdef UNIT_TESTING
extern void* _test_calloc(const size_t num, const size_t size, const char* file,
    const int line);

#define calloc(num, size) _test_calloc(num, size, __FILE__, __LINE__)
#endif

/*
 * DoubleLinkList_AppendBack
 *
 * Insert an element at the beginning of double linked list
 * `data` any data passed by void pointer
 * `bSeek` - set the pHead pointer to the inserted node
 *
 * NOTE: The caller manages the cleanup and memory deallocation
 * */
void DoubleLinkList_AppendBack(DOUBLE_LINK_LIST *list, void *data, BOOL bSeek)
{
  DOUBLE_LINK_NODE *node = calloc(1, sizeof(DOUBLE_LINK_NODE));
  assert(node);
  node->pData = data;

  if (!list->pEnd) {
    list->pEnd = node;
  }

  if (bSeek) {
    list->pHead = node;
  }

  if (list->pBegin) {
    list->pBegin->pPrev = node;
    node->pNext = list->pBegin;
  }

  list->pBegin = node;
}

/*
 * DoubleLinkList_AppendFront
 *
 * Insert an element at the end of double linked list
 * `data` any data passed by void pointer
 * `bSeek` - set the pHead pointer to the inserted node
 *
 * NOTE: The caller manages the cleanup and memory deallocation
 * */
void DoubleLinkList_AppendFront(DOUBLE_LINK_LIST *list, void *data, BOOL bSeek)
{
  DOUBLE_LINK_NODE *node = calloc(1, sizeof(DOUBLE_LINK_NODE));
  assert(node);
  node->pData = data;

  if (!list->pBegin) {
    list->pBegin = node;
  }

  if (bSeek) {
    list->pHead = node;
  }

  if (list->pEnd) {
    list->pEnd->pNext = node;
    node->pPrev = list->pEnd;
  }

  list->pEnd = node;
}

/*
 * DoubleLinkList_Sort
 *
 * Sort the provided double linked list by an algorithm provided in pfnSort
 * callback function
 *
 * Note: if pfnSort is not provided, the function will fail
 * */
void DoubleLinkList_Sort(DOUBLE_LINK_LIST *list) {
  if (!list->pfnSort) {
    assert(FALSE);
    return;
  }

  /* List is empty, nothing to sort */
  if (!list->pBegin)
    return;

  for (DOUBLE_LINK_NODE *node = list->pBegin; node; node = node->pNext) {
    for (DOUBLE_LINK_NODE *index = node->pNext; index; index = index->pNext) {
      if (list->pfnSort(node->pData, index->pData)) {
        void *temp = node->pData;
        node->pData = index->pData;
        index->pData = temp;
      }
    }
  }
}

