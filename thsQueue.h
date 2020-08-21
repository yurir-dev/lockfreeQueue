#pragma once

#include <atomic>
#include "spinlock_mutex.h"

#include <iostream>
#include <mutex>

template <class T, size_t N, size_t ThreadNum>
class queueBase
{
public:
	queueBase() = default;
	virtual ~queueBase() = default;

	template <typename U>
	bool push(U&& v)
	{
		if (m_writeHead.load() - m_readHead.load() > N)
			return false; // no place

		// it's possible that queue is almost full (one place left) and ThreadNum of threads entered,
		// it still has enough place to store all values.

		// many threads write in paralel
		const uint64_t ind = m_writeHead.fetch_add(1);
		write(ind, std::forward<U>(v));

		// increment tail in the right order
		while (m_writeTail.load() != ind);
		++m_writeTail;

		return true;
	}

protected:
	constexpr size_t sizeofArr()const { return sizeof(m_ringBuffer) / sizeof(m_ringBuffer[0]); }

	void write(uint64_t ind, T& v)
	{
		m_ringBuffer[ind % sizeofArr()] = v;
	}
	void write(uint64_t ind, T&& v)
	{
		m_ringBuffer[ind % sizeofArr()] = std::move(v);
	}

	T m_ringBuffer[N + ThreadNum + 1];
	std::atomic<uint64_t> m_writeHead{ 0 };
	std::atomic<uint64_t> m_writeTail{ 0 };
	std::atomic<uint64_t> m_readHead{ 0 };
	std::atomic<uint64_t> m_readTail{ 0 };

private:
	queueBase(const queueBase&) = delete;
	queueBase(const queueBase&&) = delete;
	queueBase& operator=(const queueBase&) = delete;
	queueBase& operator=(const queueBase&&) = delete;

	
};



/*
	Many producers to One consumer queue

	N - queue size
	ThreadNum - max number of threads using Q
*/
template <class T, size_t N, size_t ThreadNum>
class m2oQueue : public queueBase<T, N, ThreadNum>
{
public:
	m2oQueue() {}
	~m2oQueue() {}

	bool pop(T& out_v)
	{
		if(m_readHead.load() == m_writeTail.load())
			return false; // empty

		out_v = std::move(m_ringBuffer[m_readHead.load() % sizeofArr()]);
		++m_readHead;

		return true;
	}
};




/*
	Many producers to Many consumers queue

	N - queue size
	ThreadNum - max number of threads using Q
*/
template <class T, size_t N, size_t ThreadNum>
class m2mQueue : public queueBase<T, N, ThreadNum>
{
public:
	m2mQueue() = default;
	~m2mQueue() = default;

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
		out_v = std::move(m_ringBuffer[ind % sizeofArr()]);

		while (m_readTail.load() != ind);
		++m_readTail;

		return true;
	}

private:

	spinlock_mutex m_mtx;
};

