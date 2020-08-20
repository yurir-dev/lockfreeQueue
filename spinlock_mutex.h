#pragma once

#include <atomic>

/*
	taken from the book - C++ concurrency in action, 
	by Anthony Williams
	section 7 Designing Lock-Free concurrent data structures, page 181
*/
class spinlock_mutex
{
public:
	spinlock_mutex() = default;
	~spinlock_mutex()
	{
		unlock();
	}

	void lock()
	{
		while (m_flag.test_and_set(std::memory_order_acquire));
	}
	void unlock()
	{
		m_flag.clear(std::memory_order_release);
	}

private:
	std::atomic_flag m_flag{ ATOMIC_FLAG_INIT };
};

// tried with std::lock_guard<spinlock_mutex>, but performance twice better with this one
template<class mtx_t>
class genLock final
{
public:
	genLock(mtx_t &m): mtx(m)
	{
		mtx.lock();
	}
	~genLock()
	{
		mtx.unlock();
	}
	mtx_t& mtx;
};