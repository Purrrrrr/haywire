cmake_minimum_required (VERSION 2.6)
project (test)

add_definitions(-std=gnu99 -pedantic)

add_executable(test test.c)
target_link_libraries (test ncurses)

add_library(logs logs.c parse_log.c)
target_link_libraries (logs ghthash)
target_link_libraries (test logs)
#add_library(SList SList.cpp)
#target_link_libraries (tester SList)

#file( GLOB TEST_SRCS tests/*.cpp)
#add_library(Testsuites ${TEST_SRCS})
#target_link_libraries (tester Testsuites)
