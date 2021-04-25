
add_executable(out main.cc)

target_include_directories(out PRIVATE "${Boost_INCLUDE_DIRS}")
target_link_directories(out PRIVATE "${Boost_LIBRARY_DIRS}")
target_link_libraries(out Threads::Threads)
message("${Boost_LIBRARY_DIRS}")
message("${Boost_INCLUDE_DIR}")
message("${Boost_INCLUDE_DIRS}")

set_property(TARGET out PROPERTY CXX_STANDARD 17)
