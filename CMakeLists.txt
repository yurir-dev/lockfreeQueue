cmake_minimum_required(VERSION 3.10)

set (PROJECT_NAME "lockFreeQueue")
set (EXE_NAME "lockFreeQueue")

# set the project name
project(${PROJECT_NAME})

set (SOURCES main.cpp thsQueue.h tests.h tests.cpp stdQueueLock.h)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# add the executable
add_executable(${EXE_NAME} ${SOURCES})

