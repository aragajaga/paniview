cmake_minimum_required(VERSION 3.17 FATAL_ERROR)

project(paniview LANGUAGES C CXX)

include(CTest)

# Generate PE private build string
set(BUILDSTRING "Built by: $ENV{USER} on <$ENV{NAME}>")

find_package(Git)
find_package(GLEW REQUIRED)

if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    RESULT_VARIABLE GIT_COMMITHASH_RESULT
    RESULT_VARIABLE GIT_COMMITHASH_OUTPUT
    OUTPUT_QUIET
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  # If git command succeed
  if(GIT_COMMITHASH_RESULT EQUAL 0)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} config user.name
      RESULT_VARIABLE GIT_USERNAME_RESULT
      OUTPUT_VARIABLE GIT_USERNAME_OUTPUT
      OUTPUT_QUIET
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE)

    execute_process(
      COMMAND ${GIT_EXECUTABLE} config user.email
      RESULT_VARIABLE GIT_EMAIL_RESULT
      OUTPUT_VARIABLE GIT_EMAIL_OUTPUT
      OUTPUT_QUIET
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE)

    set(BUILDSTRING "Built by: ${GIT_USERNAME_OUTPUT} <${GIT_EMAIL_OUTPUT}>")
  else()
    message("[VersionInfo::PrivateBuildString] No Git repository.")
  endif()

endif()

configure_file(version.h.in version.h)
add_executable(${PROJECT_NAME} WIN32 paniview.c dlnklist.c hashmap.c d2dwrapper.cpp paniview.rc)
target_compile_definitions(${PROJECT_NAME} PRIVATE _UNICODE UNICODE)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(MSVC)
target_compile_options(${PROJECT_NAME} PRIVATE /W4 /Wall /WX /wd6031 /wd28251 /wd4820 /wd5045 /wd4701 /wd4703)
target_link_options(${PROJECT_NAME} PRIVATE /MANIFEST:NO)
else()
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror -pedantic)
target_link_options(${PROJECT_NAME} PRIVATE -municode)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE comctl32 shlwapi pathcch d2d1 windowscodecs GLEW::GLEW)

if(BUILD_TESTING)
  find_package(cmocka 1.1.7 REQUIRED)

  set(TEST_TARGETS
    test_double_link_list
    test_hash_map
  )
  
  foreach(TEST_TARGET ${TEST_TARGETS})
    add_executable(${TEST_TARGET} ${CMAKE_CURRENT_SOURCE_DIR}/test/${TEST_TARGET}.c ${CMAKE_CURRENT_SOURCE_DIR}/dlnklist.c ${CMAKE_CURRENT_SOURCE_DIR}/hashmap.c)
    target_link_libraries(${TEST_TARGET} PRIVATE cmocka::cmocka)
    add_test(NAME ${TEST_TARGET} COMMAND ${TEST_TARGET})
    if(TEST_ENV)
      set_tests_properties(${TEST_TARGET} PROPERTIES ENVIRONMENT "PATH=${TEST_ENV}")
    endif()
  endforeach()

endif()
