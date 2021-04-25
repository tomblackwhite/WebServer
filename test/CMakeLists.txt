
add_executable(test_unit test.cc)
message("${Boost_INCLUDE_DIRS}\n")
target_include_directories(test_unit PRIVATE
  "${Boost_INCLUDE_DIRS}"
  "${PROJECT_SOURCE_DIR}/include")


add_test(NAME test COMMAND test_unit)
