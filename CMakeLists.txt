cmake_minimum_required (VERSION 2.6)
add_definitions(-std=gnu99 -pedantic -g)
add_definitions(`getconf LFS_CFLAGS` -D_LARGEFILE64_SOURCE)
add_definitions(`getconf LFS_LDFLAGS`)
project (haywire)

add_executable(haywire haywire.c)
target_link_libraries (haywire ncurses)
target_link_libraries (haywire logs)
target_link_libraries (haywire appstate)


add_executable(test test.c)
target_link_libraries (test logs)
target_link_libraries (test appstate)

add_library(appstate appstate.c)
target_link_libraries (appstate ncurses)

add_library(logs logs.c)
target_link_libraries (logs ghthash)
target_link_libraries (logs linereader)
target_link_libraries (logs parselog)

add_library(parselog parse_log.c)

add_library(linereader linereader.c)

add_library(ghthash ghthash/hash_table.c)
target_link_libraries (ghthash ghthash_hashfunctions)

add_library(ghthash_hashfunctions ghthash/hash_functions.c)

#add_library(SList SList.cpp)
#target_link_libraries (tester SList)

#file( GLOB TEST_SRCS tests/*.cpp)
#add_library(Testsuites ${TEST_SRCS})
#target_link_libraries (tester Testsuites)
