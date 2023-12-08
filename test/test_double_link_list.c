#include "../precomp.h"
#include "../dlnklist.h"

#include <setjmp.h>
#include <cmocka.h>

typedef struct _tagDUMMYDATA {
  int nNumber;
  WCHAR szDummy[80];
} DUMMYDATA;

static void double_link_list_heap_test(void** state)
{
  UNREFERENCED_PARAMETER(state);

  DOUBLELINKLIST linkList;
  DoubleLinkList_Init(&linkList, NULL);

  int val;
  
  val = 30;
  DoubleLinkList_AppendFront(&linkList, &val, sizeof(int), TRUE);
  
  val = 10;
  DoubleLinkList_AppendFront(&linkList, &val, sizeof(int), TRUE);

  val = 45;
  DoubleLinkList_AppendFront(&linkList, &val, sizeof(int), TRUE);
}

struct word_test_pair {
  unsigned int order;
  wchar_t word[80];
};

/*
 * EDIT CAREFULLY OR DON'T EDIT AT ALL
 * Do not mess alphabetical order, better use script for check/generation
 * */
const struct word_test_pair g_unorderedWordSet[][80] = {
  {  8, L"abandonment" },
  { 15, L"rejecton" },
  { 17, L"Чаща" },
  {  4, L"FEAR" },
  { 14, L"loneliness" },
  { 22, L"痛み" },
  { 19, L"цапля" },
  { 10, L"desertion" },
  {  6, L"Panit.ent" },
  {  9, L"depression" },
  { 21, L"放棄" },
  {  2, L"ANXIETY" },
  { 12, L"hopelessness" },
  { 11, L"grimstroke-core" },
  { 20, L"恐れ" },
  {  0, L"0zero" },
  { 23, L"絶望" },
  {  5, L"Pain" },
  { 16, L"unhappiness" },
  {  1, L"9chirno" },
  {  3, L"Despair" },
  { 18, L"булава" },
  {  7, L"__padding" },
  { 13, L"imminent" },
};

static BOOL WstringTestPairComparator(const void* p1, size_t size1, const void* p2, size_t size2)
{
  return wcscmp(
    ((struct word_test_pair*)p1)->word,
    ((struct word_test_pair*)p2)->word) > 0;
}

static void double_link_list_wstring_sort_test(void** state)
{
  DOUBLELINKLIST list;
  DoubleLinkList_Init(&list, WstringTestPairComparator);

  for (size_t i = 0; i < ARRAYSIZE(g_unorderedWordSet); ++i) {
    DoubleLinkList_AppendFront(&list, &g_unorderedWordSet[i], sizeof(g_unorderedWordSet[i]), TRUE);
  }

  DoubleLinkList_Sort(&list);

  int i = 0;
  for (DOUBLELINKLISTNODE* node = list.pBegin; node; node = node->pNext) {
    int wordIndex = ((struct word_test_pair*)(node->pValue))->order;

    assert_int_equal(wordIndex, i++);
  }

  for (DOUBLELINKLISTNODE* node = list.pBegin; node;) {
    assert_non_null(node->pValue);
    test_free(node->pValue);
    node->pValue = NULL;

    DOUBLELINKLISTNODE* next = node->pNext;
    if (next) {
      node->pNext->pPrev = NULL;
    }

    node->pNext = NULL;

    test_free(node);
    node = next;
  }
}


const int g_unorderedNumberSet[] = {
    89, -122,  -20,   88,  111,  -36,  -95,  102,  -29,   15,  -55,  -72, -123,
  -116,  -30,   87,  -37,    7,   59,   84,   74,   29, -126,   79,  -69,   42,
    31,   78,  120, -113,  -98,  112,  -85,  -68,  101,   54, -115,  -81,   23,
    13,    0,   39,  -83,   16,  -33,  -90,   77,  -38,  -57,  -19,  -76,   37,
  -104,  123,   17,  -53,   80,   22,   19,  107,  -28,   85,   -2,  -77,   41,
  -125, -103,   90,   11,  -94,  -51,  -93,   -5,   14,   21,   73, -112,  121,
    97,  124, -111,   60,  -50,  -48, -110,   30,   83,  -52,  -89,   57,  109,
    48,  -96,  -17, -120,  -13,   -8,  -31,  -47,  -67,    9,   -7,  -16,   36,
    98, -107,  -60,   34,  -15,  128,   25,  -41,  -44,   68,  104,  105,  -25,
     6,   76,  -78,   61, -108,  -61,   28,   20,  -22,   95,    4,  -11,  -39,
   -64,   -6,  -32,  116,   70,   81,   69,   72,  -56, -124,   65,  103,   43,
   -23,   12,   71, -118,   82,   67,   46,  -34,  -54,    2,   38,   55,  -99,
    18,  126,  -79,  -26,   64,  -27,  -12,    5,  -35,  -46,   94,  110,   62,
    45,   56,  -14,  119,   75, -119,  117,   93, -121,   96,  -70,  -40,   32,
   -86,  -43,  -24,  106,   35,   66,  122, -106,   51,  -66,  100,   -3,  -88,
  -100,   99,  -75,  -63,  -49,  -21,  -45,  127,  108,  -65,  -82,  -87, -101,
   114,  -91,   92,  -59,   26,   44, -105,  -74,   52,   -9,  125,   50,  -84,
    49,    3,    1,  118,  -92,  115,   63,  -71,   47, -114,   -1, -117,   27,
  -109,   86,  -42, -127,   -4,   53,  -62,  -97,  -10, -102,  -58,   24,   10,
    40,    8,   33,  -18,  -73,   58,  113,   91,  -80
};

static int IntegerComparator(const void* p1, size_t size1, const void* p2, size_t size2)
{
  return *(int*)p1 - *(int*)p2;
}

static void double_link_list_int_sort_test(void** state)
{
  UNREFERENCED_PARAMETER(state);

  DOUBLELINKLIST list;
  DoubleLinkList_Init(&list, IntegerComparator);

  for (size_t i = 0; i < ARRAYSIZE(g_unorderedNumberSet); ++i) {
    void* data = test_calloc(1, sizeof(int));

    *(int*)data = g_unorderedNumberSet[i];
    DoubleLinkList_AppendFront(&list, &data, sizeof(int), TRUE);
  }

  DoubleLinkList_Sort(&list);

  int i = -127;
  for (DOUBLELINKLISTNODE* pNode = list.pBegin; pNode; pNode = pNode->pNext) {
    assert_int_equal(*((int*)(pNode->pValue)), i++);
  }

  for (LPDOUBLELINKLISTNODE pNode = list.pBegin; pNode;) {
    assert_non_null(pNode->pValue);
    test_free(pNode->pValue);
    pNode->pValue = NULL;

    LPDOUBLELINKLISTNODE pNext = pNode->pNext;
    if (pNext) {
      pNode->pNext->pPrev = NULL;
    }

    pNode->pNext = NULL;

    test_free(pNode);
    pNode = pNext;
  }
}

int main() {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(double_link_list_heap_test),
    cmocka_unit_test(double_link_list_int_sort_test),
    cmocka_unit_test(double_link_list_wstring_sort_test)
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
