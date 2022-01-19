#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>
#include <iostream>

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

/*
	Note do push to the tested queue,
	change N to change the Node size
*/
template <size_t N = 1>
struct Node
{
	Node(size_t v = 0)
	{
		nums.resize(N);
		std::fill(nums.begin(), nums.end(), v);
	}
	Node(const Node& rHnd)
	{
		*this = rHnd;
	}
	Node(Node&& rHnd)
	{
		*this = std::move(rHnd);
	}

	Node& operator=(const Node& rHnd)
	{
		if (this != &rHnd)
		{
			nums.resize(rHnd.nums.size());
			for (size_t i = 0; i < nums.size(); ++i)
				nums[i] = rHnd.nums[i];
		}
		return *this;
	}
	Node& operator=(Node&& rHnd)
	{
		if (this != &rHnd)
		{
			nums.resize(rHnd.nums.size());
			for (size_t i = 0; i < nums.size(); ++i)
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

	std::vector<size_t> nums;
};
using testNode = Node<12>;

template <class queue_t>
void pushFunc(queue_t& q, std::atomic<bool>& end, std::atomic<size_t>& val2push, stats& stats, std::mutex& mtx)
{
	size_t good_push{ 0 }, bad_push{ 0 };
	size_t val = 0;
	size_t lastPushed{ 0 };
	bool pushed{ true };

	while (!end.load())
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
void popFunc(queue_t& q, std::atomic<bool>& end, stats& stats, std::mutex& mtx)
{
	size_t lastPop{ 0 };

	size_t good_pop{ 0 }, bad_pop{ 0 };
	while (!end.load())
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