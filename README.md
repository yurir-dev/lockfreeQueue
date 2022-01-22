# lockfreeQueue
C++ thread safe lock free queue, implemented as a ring buffer with several atomic indexes.

m2oQueue - many producers to one consumer.

m2mQueue - many producers to many consumers.


Implementation details:

 - very simple, easy to understand implementation, written in modern C++ (at least C++11). check diagram.png for visualization of the algorithm.

 - it relies only on C++11 and \#include \<atomic> so it’s multiplatform. 

 - all queue functionality is inside lockFreeQueue.h.

 - tests/benchmarks/examples are in tests/test*.cpp 


---------------------------------------------------------------

performance comparison between lockfreeQueue and std::queue with locks

the test is pushs&pops from different threads for a specific amount of time.

each push/pop is counted and verified. 
 
the queues are limited in their size.



many producer threads to one consumer thread:

test 1 - start pushing then start pulling. ratio 1:6.5

test 2 - start pulling then start pushing. ratio 1:7.8
-----
 std:		846035,		696620

 lockfree:	5456480,	5432678

---------------

many producer threads to many consumer threads:

test 1 - start pushing then start poping. ratio 1:9.1

test 2 - start pulling then start pushing. ratio 1:10.3

test 1 - start pushing then start poping by a single consumer. ratio 1:6.9

----
 std: 1888562, 1663961, 697529

 lockfree: 17231095, 17092099, 4821644


