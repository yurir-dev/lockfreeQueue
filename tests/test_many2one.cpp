#include "lockfreeQueue.h"
#include "stdQueueLock.h"
#include "test_common.h"

#include <iostream>
#include <string>
#include <thread>

constexpr const size_t threadNum{8};

template<typename queue_t>
bool testPushPull()
{
	std::cout << " Test many threads push then one thread pulls, queue: " << typeid(queue_t).name() << std::endl;
	std::cout << "-------------------------------------------------" << std::endl;

	queue_t q;
	stats stats;

	std::atomic<size_t> val2push{ 0 };
	std::atomic<bool> endPush{ false };
	std::atomic<bool> endPop{ false };
	std::mutex mtx;

	std::vector<std::thread> threads; threads.resize(threadNum);
	for (size_t i = 0; i < threads.size() - 1; i++)
	{
		threads[i] = std::thread([&q, &endPush, &val2push, &stats, &mtx]() { pushFunc<queue_t>(q,endPush, val2push, stats, mtx); });
	}
	threads[threads.size() - 1] = std::thread([&q, &endPop, &stats, &mtx]() { popFunc<queue_t>(q, endPop, stats, mtx); });

	std::this_thread::sleep_for(std::chrono::milliseconds(10000));
	endPush.store(true);
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	endPop.store(true);

	for (auto& t : threads)
		t.join();

	std::cout << std::endl;
	std::cout << "totalPops: " << stats.totalPull.load() << ", total push: " << stats.totalPush.load() << " , verdict : " << (stats.totalPush.load() == stats.totalPull.load() ? "OK" : "FAIL") << std::endl;
	std::cout << "pushed sum: " << stats.pushSum.load() << ", pulled sum: " << stats.pullSum.load() << " , verdict : " << (stats.pushSum.load() == stats.pullSum.load() ? "OK" : "FAIL") << std::endl;

	std::cout << " ---- End ----" << std::endl << std::endl << std::endl;

	bool res = (stats.totalPush.load() == stats.totalPull.load()) && (stats.pushSum.load() == stats.pullSum.load());
	return res;
	return false;
}

template<typename queue_t>
bool testPullPush()
{
	std::cout << " Test : one thread pulls then many threads push, queue: " << typeid(queue_t).name() << std::endl;
	std::cout << "-------------------------------------------------" << std::endl;

	queue_t q;
	stats stats;

	std::atomic<size_t> val2push{ 0 };
	std::atomic<bool> endPush{ false };
	std::atomic<bool> endPop{ false };
	std::mutex mtx;

	std::vector<std::thread> threads; threads.resize(threadNum);
	threads[0] = std::thread([&q, &endPop, &stats, &mtx]() { popFunc<queue_t>(q, endPop, stats, mtx); });

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	for (size_t i = 1; i < threads.size(); i++)
	{
		threads[i] = std::thread([&q, &endPush, &val2push, &stats, &mtx]() { pushFunc<queue_t>(q, endPush, val2push, stats, mtx); });
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(10000));
	endPush.store(true);
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	endPop.store(true);

	for (auto& t : threads)
		t.join();

	std::cout << std::endl;
	std::cout << "totalPops: " << stats.totalPull.load() << ", total push: " << stats.totalPush.load() << " , verdict : " << (stats.totalPush.load() == stats.totalPull.load() ? "OK" : "FAIL") << std::endl;
	std::cout << "pushed sum: " << stats.pushSum.load() << ", pulled sum: " << stats.pullSum.load() << " , verdict : " << (stats.pushSum.load() == stats.pullSum.load() ? "OK" : "FAIL") << std::endl;

	std::cout << " ---- End ----" << std::endl << std::endl << std::endl;

	bool res = (stats.totalPush.load() == stats.totalPull.load()) && (stats.pushSum.load() == stats.pullSum.load());
	return res;
}

int main(int argc, char* argv[])
{
	if (argc == 2 && std::string(argv[1]) == "std")
	{
		using m2oStdQueueWithLocks = stdQueueWithLocks<testNode, 128>;

		if (!testPushPull<m2oStdQueueWithLocks>())
			return __LINE__;
		if (!testPullPush<m2oStdQueueWithLocks>())
			return __LINE__;
	}

	using m2oQueue_t = concurency::m2oQueue<testNode, 128, threadNum>;

	if (!testPushPull<m2oQueue_t>())
		return __LINE__;
	if (!testPullPush<m2oQueue_t>())
		return __LINE__;

	return 0;
}

/*

 Test many threads push then one thread pulls, queue: 17stdQueueWithLocksI4NodeILm12EELm128EE
-------------------------------------------------
totalPops: 846035, total push: 846035 , verdict : OK

 Test : one thread pulls then many threads push, queue: 17stdQueueWithLocksI4NodeILm12EELm128EE
-------------------------------------------------
totalPops: 696620, total push: 696620 , verdict : OK

 Test many threads push then one thread pulls, queue: N10concurency8m2oQueueI4NodeILm12EELm128ELm8EEE
-------------------------------------------------
totalPops: 5456480, total push: 5456480 , verdict : OK

 Test : one thread pulls then many threads push, queue: N10concurency8m2oQueueI4NodeILm12EELm128ELm8EEE
-------------------------------------------------
totalPops: 5432678, total push: 5432678 , verdict : OK

*/