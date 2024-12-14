cmake_minimum_required(VERSION 3.10)


include_directories(../.)
include_directories(./.)

#include_directories(${CMAKE_SOURCE_DIR} . ../ )

# Files common to all tests
set (COMMON_SOURCES ../lockfreeQueue.h ./test_common.h ./stdQueueLock.h)

set(TEST_INTERFACE test_interface)
add_executable(${TEST_INTERFACE} test_interface.cpp ${COMMON_SOURCES})

set(TEST_MANY2ONE test_many2one)
add_executable(${TEST_MANY2ONE} test_many2one.cpp ${COMMON_SOURCES})

set(TEST_MANY2MANY test_many2many)
add_executable(${TEST_MANY2MANY} test_many2many.cpp ${COMMON_SOURCES})

set(TEST_ATOMICS test_atomics)
add_executable(${TEST_ATOMICS} test_atomics.cpp ${COMMON_SOURCES})

set(TEST_QUEUEBUFFER test_queueBuffer)
add_executable(${TEST_QUEUEBUFFER} test_queueBuffer.cpp ${COMMON_SOURCES})

set(exes ${TEST_INTERFACE} ${TEST_MANY2ONE} ${TEST_MANY2MANY} ${TEST_ATOMICS} ${TEST_QUEUEBUFFER})

if (UNIX)
message("creating linux project")
foreach (exe IN LISTS exes)
	target_link_libraries(${exe} pthread)
endforeach()
endif()

foreach (exe IN LISTS exes)
	add_test(${exe} ${exe})
endforeach()
