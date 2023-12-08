/*
 * dlnklist.h
 *
 * A doubly linked list implementation
 */

#ifndef PANIVIEW_DLNKLIST_H
#define PANIVIEW_DLNKLIST_H

typedef struct _tagDOUBLELINKLISTNODE DOUBLELINKLISTNODE, *LPDOUBLELINKLISTNODE;
typedef struct _tagDOUBLELINKLIST DOUBLELINKLIST, *LPDOUBLELINKLIST;
typedef int (*DOUBLELINKLISTSORTFUNC)(const void *, size_t, const void *, size_t);

struct _tagDOUBLELINKLISTNODE {
  LPDOUBLELINKLISTNODE pNext;
  LPDOUBLELINKLISTNODE pPrev;
  void *pValue;
  size_t valueSize;
};

struct _tagDOUBLELINKLIST {
  LPDOUBLELINKLISTNODE pHead;
  LPDOUBLELINKLISTNODE pBegin;
  LPDOUBLELINKLISTNODE pEnd;
  DOUBLELINKLISTSORTFUNC pfnSort;
};

void DoubleLinkList_Init(LPDOUBLELINKLIST pDoubleLinkList, DOUBLELINKLISTSORTFUNC pfnSort);
LPDOUBLELINKLISTNODE DoubleLinkList_CreateNode(void);
void DoubleLinkList_AppendBack(LPDOUBLELINKLIST pDoubleLinkList, const void* pValue, size_t valueSize, BOOL bSeek);
void DoubleLinkList_AppendFront(LPDOUBLELINKLIST pDoubleLinkList, const void* pValue, size_t valueSize, BOOL bSeek);
void DoubleLinkList_Sort(LPDOUBLELINKLIST pDoubleLinkList);

#endif /* PANIVIEW_DLNKLIST_H */
