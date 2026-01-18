#pragma once

#include <array>
#include <memory>
#include <atomic>
#include <immintrin.h> // For _mm_pause on x86

namespace concurency_2026{

struct alignas(64) SharedCnt
{
    std::atomic<size_t> _cnt{0};
};

template<typename T, size_t N>
class QueueSPSC
{
    public:   
    QueueSPSC() = default;
    ~QueueSPSC() = default;

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

}