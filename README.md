# lockfreeQueue
C++ thread safe lock free queue, implemented as a ring buffer with several atomic indexes.

m2oQueue - many producers to one consumer.

m2mQueue - many producers to many consumers.

Implementation details:

very simple, easy to understand implementation, written in modern C++ (at least C++11). check diagram.png for visualization of the algorithm.

it relies only on C++ STD library so it’s multiplatform. 

all queue functionality is inside thsQueue.h.


