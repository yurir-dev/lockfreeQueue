#pragma once

#include "thsQueue.h"

template <class T, size_t N, size_t ThreadNum>
m2oQueue<T, N, ThreadNum>::m2oQueue()
{}
template <class T, size_t N, size_t ThreadNum>
m2oQueue<T, N, ThreadNum>::~m2oQueue()
{}

template <class T, size_t N, size_t ThreadNum>
bool m2oQueue<T, N, ThreadNum>::push(const T& v)
{
	return false;
}
template <class T, size_t N, size_t ThreadNum>
bool m2oQueue<T, N, ThreadNum>::pop(T& out_v)
{
	return false;
}
