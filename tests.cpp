#include "tests.h"

//#define USE_TO_STD_VERSION

#if !defined(USE_TO_STD_VERSION)
#include "thsQueue.h"
#else
#include "stdQueueLock.h"
#endif

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <queue>
#include <atomic>

#define SIZEOF_ARRAY(__arr__) (sizeof(__arr__) / sizeof(__arr__[0]))



/*
	Note do push to the tested queue,
	change N to change the Node size
*/
template <size_t N = 1>
struct Node
{
	Node(size_t n = 0)
	{
		for (size_t i = 0; i < SIZEOF_ARRAY(nums) ; ++i)
			nums[i] = n;
	}
	Node(const Node<N>& rHnd)
	{
		*this = rHnd;
	}
	Node(const Node<N>&& rHnd)
	{
		*this = std::move(rHnd);
	}

	Node& operator=(const Node<N>& rHnd)
	{
		if (this != &rHnd)
		{
			for (size_t i = 0; i < SIZEOF_ARRAY(nums); ++i)
				nums[i] = rHnd.nums[i];
		}
		return *this;
	}
	Node& operator=(const Node<N>&& rHnd)
	{
		if (this != &rHnd)
		{
			for (size_t i = 0; i < SIZEOF_ARRAY(nums); ++i)
				nums[i] = rHnd.nums[i];
		}
		return *this;
	}

	size_t val() const { return nums[0]; }
	bool verify()const
	{
		size_t val = nums[0];
		for (auto x : nums)
			if (val != x)
				return false;
		return true;
	}

	size_t nums[N] = {0};
};
using testNode = Node<12>;


struct stats
{
	std::atomic<size_t> totalPush{ 0 };
	std::atomic<size_t> pushSum{ 0 };
	std::atomic<size_t> totalPull{ 0 };
	std::atomic<size_t> pullSum{ 0 };

	void reset()
	{
		totalPush.store(0);
		pushSum.store(0);
		totalPull.store(0);
		pullSum.store(0);
	}
};

using m2oQueueTest = m2oQueue<testNode, 1024, 32>;
m2oQueueTest m2oQueueObj;

using m2mQueueTest = m2mQueue<testNode, 1024, 32>;
m2mQueueTest m2mQueueObj;

std::mutex mtx;
std::atomic<bool> endPush{ false };
std::atomic<bool> endPop{ false };
std::atomic<size_t> val2push{ 0 };
const size_t threadNum = std::thread::hardware_concurrency();



bool testInterface()
{
	std::cout << __FUNCTION__ << " Test : interfaces" << std::endl;
	std::cout << "-------------------------------------------------" << std::endl;

	bool res = true;
	{
		m2oQueue<testNode, 12, 0> q;
		q.push(testNode(1)); //move

		testNode n;
		q.pop(n); // move

		q.push(n); // copy

		q.push(std::move(n)); // move

		bool r = (n.val() == 1);
		std::cout << __FUNCTION__ << " testNode: verdict: " << (r ? "OK" : "FAIL") << std::endl;

		res = res & r;
	}

	{
		m2oQueue<int, 12, 0> q;
		q.push(1);

		int n;
		q.pop(n);

		bool r = (n == 1);
		std::cout << __FUNCTION__ << " int: verdict: " << (r ? "OK" : "FAIL") << std::endl;

		res = res & r;
	}

	{
		m2oQueue<std::string, 12, 0> q;
		q.push("cucu");

		std::string v;
		q.pop(v);

		bool r = (v == "cucu");
		std::cout << __FUNCTION__ << " std::string: verdict: " << (r ? "OK" : "FAIL") << std::endl;

		res = res & r;
	}

	std::cout << " ---- End ----" << std::endl << std::endl << std::endl;

	return res;
}




template <class queue_t>
static void pushFunc(stats& stats, queue_t &q)
{
	size_t good_push{ 0 }, bad_push{ 0 };
	size_t val = 0;
	size_t lastPushed{ 0 };
	bool pushed{ true };

	while (!endPush.load())
	{
		if (pushed)
			val = val2push.fetch_add(1);

		pushed = q.push(testNode(val));
		if (pushed)
		{
			lastPushed = val;
			stats.pushSum += lastPushed;
			good_push++;
		}
		else
			bad_push++;
	}

	stats.totalPush += good_push;

	std::lock_guard<std::mutex> l(mtx);
	std::cout << "pusher: good_pushs: " << good_push << ", bad_pushs: " << bad_push << ", last pushed: " << lastPushed << std::endl;

}

template <class queue_t>
static void popFunc(stats& stats, queue_t& q)
{
	size_t lastPop{ 0 };

	size_t good_pop{ 0 }, bad_pop{ 0 };
	while (!endPop.load())
	{
		testNode n;
		if (q.pop(n))
		{
			good_pop++;
			lastPop = n.val();
			stats.pullSum += lastPop;

			if (!n.verify())
				std::cout << "poper: verification failed" << std::endl;
		}
		else
			bad_pop++;
	}

	stats.totalPull += good_pop;

	std::lock_guard<std::mutex> l(mtx);
	std::cout << "poper: good_pops: " << good_pop << ", bad_pops: " << bad_pop << ", last pop: " << lastPop << std::endl;
}

bool testM2OQueue_ManyThreads_Pushes_1thread_Pops()
{
	std::cout << __FUNCTION__ << " Test : Many pushes then pulls" << std::endl;
	std::cout << "-------------------------------------------------" << std::endl;

	stats stats;

	val2push.store(0);
	endPop.store(false);
	endPush.store(false);

	std::vector<std::thread> threads; threads.resize(threadNum);
	for (size_t i = 0; i < threads.size() - 1; i++)
	{
		threads[i] = std::thread([&stats]() { pushFunc<m2oQueueTest>(stats, m2oQueueObj); });
	}
	threads[threads.size() - 1] = std::thread([&stats]() { popFunc<m2oQueueTest>(stats, m2oQueueObj); });

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
bool testM2OQueue_1thread_Pops_ManyThreads_Pushes()
{
	std::cout << __FUNCTION__ << " Test : pulls then many pushes" << std::endl;
	std::cout << "-------------------------------------------------" << std::endl;

	stats stats;

	val2push.store(0);
	endPop.store(false);
	endPush.store(false);

	std::vector<std::thread> threads; threads.resize(threadNum);
	threads[0] = std::thread([&stats]() { popFunc<m2oQueueTest>(stats, m2oQueueObj); });

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	for (size_t i = 1; i < threads.size(); i++)
	{
		threads[i] = std::thread([&stats]() { pushFunc<m2oQueueTest>(stats, m2oQueueObj); });
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


bool testM2MQueue_ManyThreads_Pushes_ManyThreads_Pops()
{
	stats stats;

	std::cout << __FUNCTION__ << " Test : Many pushes then pulls" << std::endl;
	std::cout << "-------------------------------------------------" << std::endl;

	stats.reset();
	val2push.store(0);
	endPop.store(false);
	endPush.store(false);

	std::vector<std::thread> threads; threads.resize(threadNum);
	for (size_t i = 0; i < threads.size() / 2; i++)
		threads[i] = std::thread([&stats]() { pushFunc<m2mQueueTest>(stats, m2mQueueObj); });

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	for (size_t i = threads.size() / 2; i < threads.size(); i++)
		threads[i] = std::thread([&stats]() { popFunc<m2mQueueTest>(stats, m2mQueueObj); });

	std::this_thread::sleep_for(std::chrono::milliseconds(10000));
	endPush.store(true);
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	endPop.store(true);

	for (auto& t : threads)
	{
		if(t.joinable())
			t.join();
	}
	std::cout << std::endl;
	std::cout << "totalPops: " << stats.totalPull.load() << ", total push: " << stats.totalPush.load() << " , verdict : " << (stats.totalPush.load() == stats.totalPull.load() ? "OK" : "FAIL") << std::endl;
	std::cout << "pushed sum: " << stats.pushSum.load() << ", pulled sum: " << stats.pullSum.load() << " , verdict : " << (stats.pushSum.load() == stats.pullSum.load() ? "OK" : "FAIL") << std::endl;

	std::cout << " ---- End ----" << std::endl << std::endl << std::endl;
	
	bool res = (stats.totalPush.load() == stats.totalPull.load()) && (stats.pushSum.load() == stats.pullSum.load());
	return res;
}

bool testM2MQueue_ManyThreads_Pops_ManyThreads_Pushes()
{
	std::cout << __FUNCTION__ << " Test : many pulls then many pushes" << std::endl;
	std::cout << "-------------------------------------------------" << std::endl;

	stats stats;

	val2push.store(0);
	endPop.store(false);
	endPush.store(false);

	std::vector<std::thread> threads; threads.resize(threadNum);
	for (size_t i = 0; i < threads.size() / 2; i++)
		threads[i] = std::thread([&stats]() { popFunc<m2mQueueTest>(stats, m2mQueueObj); });

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	
	for (size_t i = threads.size() / 2; i < threads.size(); i++)
		threads[i] = std::thread([&stats]() { pushFunc<m2mQueueTest>(stats, m2mQueueObj); });

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

bool testM2MQueue_ManyThreads_Pushes_1thread_Pops()
{
	stats stats;

	std::cout << __FUNCTION__ << " Test : Many pushes then pulls" << std::endl;
	std::cout << "-------------------------------------------------" << std::endl;

	stats.reset();
	val2push.store(0);
	endPop.store(false);
	endPush.store(false);

	std::vector<std::thread> threads; threads.resize(threadNum);
	for (size_t i = 0; i < threads.size() - 1; i++)
	{
		threads[i] = std::thread([&stats]() { pushFunc<m2mQueueTest>(stats, m2mQueueObj); });
	}
	threads[threads.size() - 1] = std::thread([&stats]() { popFunc<m2mQueueTest>(stats, m2mQueueObj); });

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