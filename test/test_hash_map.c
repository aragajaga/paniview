#include "../hashmap.h"

#include <setjmp.h>
#include <cmocka.h>

static void hash_map_insert_test(void** state)
{
  (void)state;

  HASHMAP hashMap;
  InitializeHashMap(&hashMap, sizeof(int), compareInt);

  int key = 1000;
  int value = 51000;
  HashMap_Insert(&hashMap, &key, &value, sizeof(int));
  /*
   *  (1000)
   */
  assert_int_equal(1000, *(int*)hashMap.pRoot->pair.pKey);

  key = 50;
  value = 50;
  HashMap_Insert(&hashMap, &key, &value, sizeof(int));
  /*
   *    (1000)
   *    /
   *  [50]
   */
  assert_int_equal(1000, *(int*)hashMap.pRoot->pair.pKey);
  assert_int_equal(50, *(int*)hashMap.pRoot->pLeft->pair.pKey);

  key = 40;
  value = 40;
  HashMap_Insert(&hashMap, &key, &value, sizeof(int));
  /*
   *     (50)
   *     /  \
   *  [40]  [1000]
   */
  assert_int_equal(50, *(int*)hashMap.pRoot->pair.pKey);
  assert_int_equal(40, *(int*)hashMap.pRoot->pLeft->pair.pKey);
  assert_int_equal(1000, *(int*)hashMap.pRoot->pRight->pair.pKey);

  key = 80;
  value = 80;
  HashMap_Insert(&hashMap, &key, &value, sizeof(int));
  /*
   *     (50)
   *     /  \
   *  (40)  (1000)
   *        /
   *      [80]
   */
  assert_int_equal(50, *(int*)hashMap.pRoot->pair.pKey);
  assert_int_equal(40, *(int*)hashMap.pRoot->pLeft->pair.pKey);
  assert_int_equal(1000, *(int*)hashMap.pRoot->pRight->pair.pKey);
  assert_int_equal(80, *(int*)hashMap.pRoot->pRight->pLeft->pair.pKey);

  key = 1500;
  value = 1500;
  HashMap_Insert(&hashMap, &key, &value, sizeof(int));
  /*
   *     (50)
   *     /  \
   *  (40)  (1000)
   *        /   \
   *      [80]  [1500]
   */
  assert_int_equal(50, *(int*)hashMap.pRoot->pair.pKey);
  assert_int_equal(40, *(int*)hashMap.pRoot->pLeft->pair.pKey);
  assert_int_equal(1000, *(int*)hashMap.pRoot->pRight->pair.pKey);
  assert_int_equal(80, *(int*)hashMap.pRoot->pRight->pLeft->pair.pKey);
  assert_int_equal(1500, *(int*)hashMap.pRoot->pRight->pRight->pair.pKey);

  HashMap_Cleanup(&hashMap);
}

static void hash_map_get_test(void** state)
{
  (void)state;

  HASHMAP hashMap;
  InitializeHashMap(&hashMap, sizeof(int), compareInt);

  int key = 1000;
  int value = 51000;
  HashMap_Insert(&hashMap, &key, &value, sizeof(int));

  key = 50;
  value = 50;
  HashMap_Insert(&hashMap, &key, &value, sizeof(int));

  key = 40;
  value = 740;
  HashMap_Insert(&hashMap, &key, &value, sizeof(int));

  key = 80;
  value = 80;
  HashMap_Insert(&hashMap, &key, &value, sizeof(int));

  key = 1500;
  value = 1500;
  HashMap_Insert(&hashMap, &key, &value, sizeof(int));

  key = 1000;
  value = *(int*)HashMap_Get(&hashMap, &key);
  assert_int_equal(51000, value);

  key = 40;
  value = *(int*)HashMap_Get(&hashMap, &key);
  assert_int_equal(740, value);

  HashMap_Cleanup(&hashMap);
}

int main() {
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(hash_map_insert_test),
    cmocka_unit_test(hash_map_get_test)
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
