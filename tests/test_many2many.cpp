#include "lockfreeQueue.h"
#include "stdQueueLock.h"
#include "test_common.h"

#include <iostream>
#include <string>
#include <thread>


constexpr const size_t threadNum{ 8 };

template<typename queue_t>
bool testPushPop()
{
	std::cout << " Test : Many threads push then many threads pull, queue: " << typeid(queue_t).name() << std::endl;
	std::cout << "-------------------------------------------------" << std::endl;

	queue_t q;
	stats stats;

	std::atomic<size_t> val2push{ 0 };
	std::atomic<bool> endPush{ false };
	std::atomic<bool> endPop{ false };
	std::mutex mtx;

	std::vector<std::thread> threads; threads.resize(threadNum);
	for (size_t i = 0; i < threads.size() / 2; i++)
		threads[i] = std::thread([&q, &endPush, &val2push, &stats, &mtx]() { pushFunc<queue_t>(q, endPush, val2push, stats, mtx); });

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	for (size_t i = threads.size() / 2; i < threads.size(); i++)
		threads[i] = std::thread([&q, &endPop, &stats, &mtx]() { popFunc<queue_t>(q, endPop, stats, mtx); });

	std::this_thread::sleep_for(std::chrono::milliseconds(10000));
	endPush.store(true);
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	endPop.store(true);

	for (auto& t : threads)
	{
		if (t.joinable())
			t.join();
	}
	std::cout << std::endl;
	std::cout << "totalPops: " << stats.totalPull.load() << ", total push: " << stats.totalPush.load() << " , verdict : " << (stats.totalPush.load() == stats.totalPull.load() ? "OK" : "FAIL") << std::endl;
	std::cout << "pushed sum: " << stats.pushSum.load() << ", pulled sum: " << stats.pullSum.load() << " , verdict : " << (stats.pushSum.load() == stats.pullSum.load() ? "OK" : "FAIL") << std::endl;

	std::cout << " ---- End ----" << std::endl << std::endl << std::endl;

	bool res = (stats.totalPush.load() == stats.totalPull.load()) && (stats.pushSum.load() == stats.pullSum.load());
	return res;
}

template<typename queue_t>
bool testPopPush()
{
	std::cout << " Test : many threads pull then many threads push, queue: " << typeid(queue_t).name() << std::endl;
	std::cout << "-------------------------------------------------" << std::endl;

	queue_t q;
	stats stats;

	std::atomic<size_t> val2push{ 0 };
	std::atomic<bool> endPush{ false };
	std::atomic<bool> endPop{ false };
	std::mutex mtx;

	std::vector<std::thread> threads; threads.resize(threadNum);
	for (size_t i = 0; i < threads.size() / 2; i++)
		threads[i] = std::thread([&q, &endPop, &stats, &mtx]() { popFunc<queue_t>(q, endPop, stats, mtx); });

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	for (size_t i = threads.size() / 2; i < threads.size(); i++)
		threads[i] = std::thread([&q, &endPush, &val2push, &stats, &mtx]() { pushFunc<queue_t>(q, endPush, val2push, stats, mtx); });

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

template<typename queue_t>
bool testPush_1threadPop()
{
	std::cout << " Test : Many threads push then one thread pulls, queue: " << typeid(queue_t).name() << std::endl;
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
		threads[i] = std::thread([&q, &endPush, &val2push, &stats, &mtx]() { pushFunc<queue_t>(q, endPush, val2push, stats, mtx); });
	}
	threads[threads.size() - 1] = std::thread([&q, &endPop, &stats, &mtx]() { popFunc<queue_t>(q, endPop, stats, mtx); });

	std::this_thread::sleep_for(std::chrono::milliseconds(10000));
	endPush.store(true);
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	endPop.store(true);

	for (auto& t : threads)
	{
		if (t.joinable())
			t.join();
	}
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
		using m2mStdQueueWithLocks = stdQueueWithLocks<testNode, 128>;

		if (!testPushPop<m2mStdQueueWithLocks>())
			return __LINE__;
		if (!testPopPush<m2mStdQueueWithLocks>())
			return __LINE__;
		if (!testPush_1threadPop<m2mStdQueueWithLocks>())
			return __LINE__;
	}

	using m2mQueue_t = concurency::m2mQueue<testNode, 128, threadNum>;

	if (!testPushPop<m2mQueue_t>())
		return __LINE__;
	if (!testPopPush<m2mQueue_t>())
		return __LINE__;
	if (!testPush_1threadPop<m2mQueue_t>())
		return __LINE__;

	return 0;
}

/*

number of push/pulls ratio is about 1:10  std:lockfree


 Test : Many threads push then many threads pull, queue: 17stdQueueWithLocksI4NodeILm12EELm128EE
-------------------------------------------------
totalPops: 1888562, total push: 1888562 , verdict : OK

 Test : many threads pull then many threads push, queue: 17stdQueueWithLocksI4NodeILm12EELm128EE
-------------------------------------------------
totalPops: 1663961, total push: 1663961 , verdict : OK

 Test : Many threads push then one thread pulls, queue: 17stdQueueWithLocksI4NodeILm12EELm128EE
-------------------------------------------------
totalPops: 697529, total push: 697529 , verdict : OK



 Test : Many threads push then many threads pull, queue: N10concurency8m2mQueueI4NodeILm12EELm128ELm32EEE
-------------------------------------------------
totalPops: 17231095, total push: 17231095 , verdict : OK

 Test : many threads pull then many threads push, queue: N10concurency8m2mQueueI4NodeILm12EELm128ELm32EEE
-------------------------------------------------
totalPops: 17092099, total push: 17092099 , verdict : OK

 Test : Many threads push then one thread pulls, queue: N10concurency8m2mQueueI4NodeILm12EELm128ELm32EEE
-------------------------------------------------
totalPops: 4821644, total push: 4821644 , verdict : OK

*/