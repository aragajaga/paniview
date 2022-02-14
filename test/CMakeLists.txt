add_executable(paniview-test
  main.c
  ${PROJECT_SOURCE_DIR}/dlnklist.c)

target_compile_definitions(paniview-test PRIVATE UNIT_TESTING)
target_link_libraries(paniview-test cmocka)

add_test(paniview-test paniview-test)
