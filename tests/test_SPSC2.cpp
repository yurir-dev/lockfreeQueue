#include "lockfreeQueue2.h"
#include <string>
#include <thread>
#include <iostream>
#include <vector>
#include <chrono>

struct Node
{
    Node() = default;
    Node(size_t id_) : _id{id_} {}
    size_t _id{0};

    bool operator==(const Node& other_) const
    {
        return _id == other_._id;
    }
    bool operator!=(const Node& other_) const
    {
        return !operator==(other_);
    }
};
std::ostream& operator<<(std::ostream& os_, const Node& elem_)
{
    os_ << elem_._id;
    return os_;
}

bool testSPSC2()
{
    std::atomic<bool> res{true};
    concurency_2026::QueueSPSC<Node, 2> q;
    size_t numEvents{1024 * 1024};

    std::atomic<bool> sync{false};

    auto pusher = [&res, &sync, &q, numEvents](){
        std::cout << "pusher is ready" << std::endl;
        if (!q.empty())
        {
            std::cerr << "Pusher error: queue is not empty: " 
                      << std::endl;
            res = false;
        }
        while(!sync.load(std::memory_order_acquire))
        {
            _mm_pause();
        }
        for (size_t i = 0 ; i < numEvents; ++i)
        {
            Node elem{i};
            q.push(elem);
        }
    };
    auto puller = [&res, &sync, &q, numEvents](){
        std::cout << "puller is ready" << std::endl;
        while(!sync.load(std::memory_order_acquire))
        {
            _mm_pause();
        }
        for (size_t i = 0 ; i < numEvents; ++i)
        {
            Node expectedElem{i};
            Node elem;
            q.pop(elem);

            if (elem != expectedElem)
            {
                std::cerr << "Poller error: expected: " 
                          << expectedElem 
                          << ", received: " << elem
                          << std::endl;
                res = false;
                return;
            }
        }
    };

    //sync = true;
    //pusher();
    //return 0;

    std::vector<std::thread> threads;
    threads.emplace_back(puller);
    threads.emplace_back(pusher);

    std::cout << "will start the test in 2 sec" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds{2});

    sync.store(true, std::memory_order_release);

    for (auto& t : threads)
    {
        t.join();
    }

    std::cout << "finished pushing/pulling " << numEvents << " elements, res: " << res << std::endl;
    return res;
}

int main(int /*argc*/, char* /*argv*/[])
{
	if (!testSPSC2())
		return __LINE__;
    return 0;
}