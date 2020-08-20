#pragma once

#include <atomic>
#include "spinlock_mutex.h"

#include <iostream>
#include <mutex>

class nonCopyableMoveable
{
public:
	nonCopyableMoveable() = default;
	virtual ~nonCopyableMoveable() = default;
private:
	nonCopyableMoveable(const nonCopyableMoveable&) = delete;
	nonCopyableMoveable(const nonCopyableMoveable&&) = delete;
	nonCopyableMoveable& operator=(const nonCopyableMoveable&) = delete;
	nonCopyableMoveable& operator=(const nonCopyableMoveable&&) = delete;
};

/*
	Many producers to One consumer queue

	N - queue size
	ThreadNum - max number of threads using Q
*/
template <class T, size_t N, size_t ThreadNum>
class m2oQueue : private nonCopyableMoveable
{
public:
	m2oQueue() {}
	~m2oQueue() {}

	bool push(const T& v)
	{
		if (m_writeHead.load() - m_readHead.load() > N)
			return false; // no place

		// it's possible that queue is almost full (one place left) and ThreadNum of threads entered,
		// it still has enough place to store all values.

		// many threads write in paralel
		const uint64_t ind = m_writeHead.fetch_add(1);
		m_ringBuffer[ind % sizeofArr()] = v;

		// increment tail in the right order
		while (m_writeTail.load() != ind);
		++m_writeTail;

		return true;
	}
	bool pop(T& out_v)
	{
		if(m_readHead.load() == m_writeTail.load())
			return false; // empty

		out_v = m_ringBuffer[m_readHead.load() % sizeofArr()];
		++m_readHead;

		return true;
	}

private:
	constexpr size_t sizeofArr()const { return sizeof(m_ringBuffer) / sizeof(m_ringBuffer[0]); }


	T m_ringBuffer[N + ThreadNum + 1];
	std::atomic<uint64_t> m_writeHead{ 0 };
	std::atomic<uint64_t> m_writeTail{ 0 };
	std::atomic<uint64_t> m_readHead{ 0 };
};




/*
	Many producers to Many consumers queue

	N - queue size
	ThreadNum - max number of threads using Q
*/
template <class T, size_t N, size_t ThreadNum>
class m2mQueue : private nonCopyableMoveable
{
public:
	m2mQueue() = default;
	~m2mQueue() = default;

	bool push(const T& v)
	{
		if (m_writeHead.load() - m_readTail.load() > N)
			return false; // no place

		// it's possible that queue is almost full and ThreadNum of threads entered,
		// it still has enough place to store all values.

		// many threads write in paralel
		const uint64_t ind = m_writeHead.fetch_add(1);
		m_ringBuffer[ind % sizeofArr()] = v;

		// increment tail in the right order
		while (m_writeTail.load() != ind);
		++m_writeTail;

		//printState();

		return true;
	}
	bool pop(T& out_v)
	{
		uint64_t ind{ 0 };
		{
			// tried with RAII std::lock_guard and my own lock
			// performance drops drastically 
			m_mtx.lock(); // spin lock
			if (m_readHead.load() < m_writeTail.load())
			{
				ind = m_readHead.fetch_add(1);
				m_mtx.unlock();
			}
			else
			{
				m_mtx.unlock();
				return false; // empty
			}
		}

		// many threads read in paralel
		out_v = m_ringBuffer[ind % sizeofArr()];

		while (m_readTail.load() != ind);
		++m_readTail;

		//printState();

		return true;
	}

private:
	constexpr size_t sizeofArr()const { return sizeof(m_ringBuffer) / sizeof(m_ringBuffer[0]); }

	void printState()
	{
		static std::mutex mtx;
		std::lock_guard<std::mutex> l(mtx);

		std::cout 
			<< "rT: " << m_readTail.load() 
			<< ", rH: " << m_readHead.load()
			<< ", wT: " << m_writeTail.load()
			<< ", wH: " << m_writeHead.load()
			<< std::endl;
	}

	T m_ringBuffer[N + ThreadNum + 1];
	std::atomic<uint64_t> m_writeHead{ 0 };
	std::atomic<uint64_t> m_writeTail{ 0 };
	std::atomic<uint64_t> m_readHead{ 0 };
	std::atomic<uint64_t> m_readTail{ 0 };

	spinlock_mutex m_mtx;
};

