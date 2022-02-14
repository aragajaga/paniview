/*
 * dlnklist.h
 *
 * A doubly linked list implementation
 */

#ifndef PANIVIEW_DLNKLIST_H
#define PANIVIEW_DLNKLIST_H

typedef struct _tagDOUBLE_LINK_NODE DOUBLE_LINK_NODE;
typedef struct _tagDOUBLE_LINK_LIST DOUBLE_LINK_LIST;
typedef BOOL (*DLNKLIST_SORT_FUNC)(void *, void *);

struct _tagDOUBLE_LINK_NODE {
  void *pData;
  DOUBLE_LINK_NODE *pNext;
  DOUBLE_LINK_NODE *pPrev;
};

struct _tagDOUBLE_LINK_LIST {
  DOUBLE_LINK_NODE *pHead;
  DOUBLE_LINK_NODE *pBegin;
  DOUBLE_LINK_NODE *pEnd;
  DLNKLIST_SORT_FUNC pfnSort;
};

void DoubleLinkList_AppendBack(DOUBLE_LINK_LIST *, void *, BOOL);
void DoubleLinkList_AppendFront(DOUBLE_LINK_LIST *, void *, BOOL);
void DoubleLinkList_Sort(DOUBLE_LINK_LIST *);

#endif /* PANIVIEW_DLNKLIST_H */
