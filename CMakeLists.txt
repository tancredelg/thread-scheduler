cmake_minimum_required(VERSION 3.22)
project(thread_scheduler C)

set(CMAKE_C_STANDARD 11)

add_executable(test1 sut.c test1.c)
add_executable(test2 sut.c test2.c)
add_executable(test3 sut.c test3.c)
add_executable(test4 sut.c test4.c)
add_executable(test5 sut.c test5.c)
add_executable(test6 sut.c test6.c)
