#pragma once

#include <array>
#include <memory>
#include <atomic>
#include <immintrin.h> // For _mm_pause on x86
#include <type_traits>
#include <utility>

namespace concurency_2026{

struct alignas(64) SharedCnt
{
    std::atomic<size_t> _cnt{0};
};

template<typename T, size_t N>
class QueueSPSC
{
    static_assert((N > 0) && ((N & (N - 1)) == 0), "N must be a power of 2");

    public:   
    QueueSPSC() = default;
    ~QueueSPSC() = default;

    bool empty() const
    {
        const auto head{_head.load(std::memory_order_relaxed)};
        const auto tail{_tail.load(std::memory_order_acquire)};
        return head == tail;
    }

    template <typename U, typename = std::enable_if_t<std::is_assignable_v<T&, U&&>>>
    void push(U&& elem_)
    {
        const auto head{_head.load(std::memory_order_relaxed)};

        while (head - _tail.load(std::memory_order_acquire) == N)
        {
            // full
            _mm_pause();
        }

        _arr[head & (N - 1)] = std::forward<U>(elem_);

        _head.store(head + 1, std::memory_order_release);
    }

    void pop(T& elem_)
    {
        const auto tail{_tail.load(std::memory_order_relaxed)};
        while (tail == _head.load(std::memory_order_acquire))
        {
            // empty
            _mm_pause();
        }
        
        elem_ = std::move(_arr[tail & (N - 1)]);
        _tail.store(tail + 1, std::memory_order_release);
    }

    private:
    alignas(64) std::atomic<size_t> _head{0};
    alignas(64) std::atomic<size_t> _tail{0};
    alignas(64) std::array<T, N> _arr;
};

#if 0
template<typename T, size_t N>
class QueueMPSC
{
    public:   
    QueueMPSC() = default;
    ~QueueMPSC() = default;

    bool empty() const
    {
        const auto head{_head._cnt.load(std::memory_order_relaxed)};
        const auto tail{_tail._cnt.load(std::memory_order_acquire)};
        return head == tail;
    }

    void push(const T& elem_)
    {
        const auto writeInd{_head._cnt.load(std::memory_order_relaxed)};
        const auto nextInd{(writeInd + 1) % N};
        while (nextInd == _tail._cnt.load(std::memory_order_acquire))
        {
            // full
            _mm_pause();
        }

        _arr[writeInd] = elem_;

        _head._cnt.store(nextInd, std::memory_order_release);
    }

    void pop(T& elem_)
    {
        const auto readInd{_tail._cnt.load(std::memory_order_relaxed)};
        while (readInd == _head._cnt.load(std::memory_order_acquire))
        {
            // empty
            _mm_pause();
        }
        
        elem_ = _arr[readInd % N];
        _tail._cnt.store((readInd + 1) % N, std::memory_order_release);
    }

    private:
    SharedCnt _head{0};
    SharedCnt _tail{0};
    std::array<T, N> _arr;
};
#endif
}