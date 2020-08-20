#pragma once

#include <mutex>
#include <queue>
/*
	std version with locks to compare using the same test code
*/
template <class T, size_t N, size_t ThreadNum>
class m2oQueue final
{
public:
	m2oQueue() {}
	~m2oQueue() {}

	bool push(const T& v)
	{
		std::lock_guard<std::mutex> l(mtx);
		if (q.size() > N)
			return false;
		q.push(v);
		return true;
	}
	bool pop(T& out_v)
	{
		std::lock_guard<std::mutex> l(mtx);// rm it
		if (q.size() == 0)
			return false;
		out_v = q.front();
		q.pop();
		return true;
	}

private:
	std::mutex mtx;
	std::queue<T> q;
};


template <class T, size_t N, size_t ThreadNum>
class m2mQueue final
{
public:
	m2mQueue() = default;
	~m2mQueue() = default;

	bool push(const T& v)
	{
		std::lock_guard<std::mutex> l(mtx);
		if (q.size() > N)
			return false;
		q.push(v);
		return true;
	}
	bool pop(T& out_v)
	{
		std::lock_guard<std::mutex> l(mtx);
		if (q.size() == 0)
			return false;
		out_v = q.front();
		q.pop();
		return true;
	}

private:
	std::mutex mtx;
	std::queue<T> q;
};

