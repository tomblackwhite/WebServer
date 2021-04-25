
cmake_minimum_required(VERSION "3.19.1")


project(out)

#export compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS "YES")
set(CMAKE_BUILD_TYPE "Debug")
#find boost for test
#BOOST_ROOT
set(BOOST_ROOT "thirdparty/boost_1_75_0")
set(BOOST_NO_SYSTEM_PATHS "NO")
find_package(Boost 1.75.0 REQUIRED)
find_package(Threads)
#enable_testing()

add_subdirectory("src")

#add_subdirectory("test")

#libs
#add_subdirectory("src/json")
