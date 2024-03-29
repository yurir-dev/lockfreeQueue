#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace concurency
{

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
			if (this->m_readHead.load() == this->m_writeTail.load())
				return false; // empty

			out_v = std::move(this->m_ringBuffer[this->m_readHead.load() % this->sizeofArr()]);
			++this->m_readHead;

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
				if (this->m_readHead.load() < this->m_writeTail.load())
				{
					ind = this->m_readHead.fetch_add(1);
					m_mtx.unlock();
				}
				else
				{
					m_mtx.unlock();
					return false; // empty
				}
			}

			// many threads read in paralel
			out_v = std::move(this->m_ringBuffer[ind % this->sizeofArr()]);

			while (this->m_readTail.load() != ind);
			++this->m_readTail;

			return true;
		}

	private:

		spinlock_mutex m_mtx;
	};

};