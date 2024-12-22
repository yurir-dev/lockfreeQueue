#pragma once

#include <cstring>
#include <cstdint>
#include <utility>
#include <string>
#include <cmath>
#include <atomic>
#include <cassert>
#include <ostream>
#include <iostream>
#include <mutex>

class bufferQueue
{
    friend std::ostream& operator<< (std::ostream& stream, const bufferQueue& obj);
    class bufferQueueSyncMPSC;
    friend std::ostream& operator<< (std::ostream& stream, const bufferQueueSyncMPSC& obj);

    struct header
    {
        constexpr static uint64_t MagicValue{0xbadbabe};
        header(uint64_t len): _magic{MagicValue}, _len{len} {}
        bool verifyMagic() const noexcept {return _magic == MagicValue;}
        uint64_t _magic;
        uint64_t _len;
    };
    constexpr static size_t BlockSize{sizeof(header)};

    public:
    bufferQueue(size_t capacity)
    : _head{0}, _tail{0}
    {
        size_t powerOfTwo{1};
        while(powerOfTwo <= capacity)
        {
            powerOfTwo <<= 1;
        }
        _capacity = powerOfTwo;
        _buffer = new char [_capacity];
        std::memset(_buffer, 0, _capacity);
        _capacityBlocks = _capacity / BlockSize;
    }
    bufferQueue(const bufferQueue&) = delete;
    bufferQueue& operator=(const bufferQueue&) = delete;
    virtual ~bufferQueue() = default;

    bool push(const char* ptrIn, size_t len)
    {
        const auto headVal{_head.load(std::memory_order_relaxed)};
        const auto tailVal{_tail.load(std::memory_order_acquire)};

        const auto [blocksAhead, blocksOverlap] = toWriteBlocks(headVal, tailVal, _capacityBlocks);
        const auto blocksNeeded{numOfBlocks(len + sizeof(header))};
        if (blocksAhead + blocksOverlap < blocksNeeded)
        {
            return false;
        }

        auto* ptr{_buffer + headVal * BlockSize};
        new (ptr) header{len};
        ptr += sizeof(header);

        if (blocksAhead >= blocksNeeded)
        {
            std::memcpy(ptr, ptrIn, len);
        }
        else
        {
            const auto bufferAheadLen{blocksAhead * BlockSize - sizeof(header)};
            std::memcpy(ptr, ptrIn, bufferAheadLen);
            ptr = _buffer;
            const auto bufferOverlapLen{len - bufferAheadLen};
            std::memcpy(ptr, ptrIn + bufferAheadLen, bufferOverlapLen);
        }

        _head.store((headVal + blocksNeeded) % _capacityBlocks, std::memory_order_release);
   
        return true;
    }
    std::pair<const char*, size_t> front(std::string& buffer)
    {
        const auto headVal{_head.load(std::memory_order_relaxed)};
        const auto tailVal{_tail.load(std::memory_order_acquire)};

        if (empty(headVal, tailVal))
        {
            return {nullptr, 0};
        }

        const auto* ptr{_buffer + tailVal * BlockSize};
        const auto* headerPtr{reinterpret_cast<const header*>(ptr)};
        if (!headerPtr->verifyMagic())
        {
            assert(false && "corrupted data, failed to verify magic");

        }
        
        ptr += sizeof(header);

        const auto [blocksAhead, blocksOverlap] = toReadBlocks(headVal, tailVal, _capacityBlocks);
        if (headerPtr->_len + sizeof(header) <= blocksAhead * BlockSize)
        {
            return {ptr, headerPtr->_len};
        }

        buffer.resize(headerPtr->_len);
        const auto bufferAheadLen{blocksAhead * BlockSize - sizeof(header)};
        std::memcpy(buffer.data(), ptr, bufferAheadLen);
        ptr = _buffer;
        const auto bufferOverlapLen{headerPtr->_len - bufferAheadLen};
        std::memcpy(buffer.data() + bufferAheadLen, ptr, bufferOverlapLen);

        assert(bufferOverlapLen + bufferAheadLen == headerPtr->_len);

        return {buffer.c_str(), buffer.size()};
    }
    bool pop()
    {
        const auto headVal{_head.load(std::memory_order_relaxed)};
        const auto tailVal{_tail.load(std::memory_order_acquire)};

        if (empty(headVal, tailVal))
        {
            return false;
        }

        const auto* ptr{_buffer + tailVal * BlockSize};
        const auto* headerPtr{reinterpret_cast<const header*>(ptr)};
        assert(headerPtr->verifyMagic());

        const auto blocksToSkip{numOfBlocks(headerPtr->_len + sizeof(header))};

        _tail.store((tailVal + blocksToSkip) % _capacityBlocks, std::memory_order_release);

        return true;
    }

    bool empty() const noexcept
    {
        const auto headVal{_head.load(std::memory_order_relaxed)};
        const auto tailVal{_tail.load(std::memory_order_acquire)};

        return empty(headVal, tailVal);
    }
    static bool empty(size_t headVal, size_t tailVal) noexcept
    {
        return headVal == tailVal;
    }
    bool full() const noexcept
    {
        const auto headVal{_head.load(std::memory_order_relaxed)};
        const auto tailVal{_tail.load(std::memory_order_acquire)};

        return full(headVal, tailVal, _capacityBlocks);
    }
    static bool full(size_t headVal, size_t tailVal, size_t capacityBlocks) noexcept
    {
        return (headVal + 1) % capacityBlocks == tailVal;
    }

    private:
    static std::pair<size_t, size_t> toWriteBlocks(size_t head, size_t tail, size_t capacityBlocks)
    {
        /*
        [* * * tail - - - head * * *]
        */
        if (head >= tail)
        {
            if (tail == 0)
            {
                return {capacityBlocks - head - 1, 0};
            }
            return {capacityBlocks - head, tail - 1};
        }

        /*
        [- - - head * * * tail - - -]
        */
       return {tail - head - 1, 0};
    }
    static std::pair<size_t, size_t> toReadBlocks(size_t head, size_t tail, size_t capacityBlocks)
    {
        /*
        [- - - tail * * * head - - -]
        */
        if (head >= tail)
        {
            return {head - tail, 0};
        }

        /*
        [* * * head - - - tail * * *]
        */
       return {capacityBlocks - tail, head};
    }
    static size_t numOfBlocks(size_t n)
    {
        return std::ceil(static_cast<double>(n) / static_cast<double>(BlockSize));
    }
   
    protected:
    std::atomic<size_t> _head;
    std::atomic<size_t> _tail;
    char* _buffer;
    size_t _capacity;
    size_t _capacityBlocks;
};

std::ostream& operator<< (std::ostream& stream, const bufferQueue& obj)
{
    stream << "_head: " << obj._head << ", _tail: " << obj._tail
           << ", _capacity: " << obj._capacity 
           << ", _capacityBlocks: " << obj._capacityBlocks;
    return stream;
}

using bufferQueueSyncSPSC = bufferQueue;

class bufferQueueSyncMPSC : protected bufferQueue
{
    friend std::ostream& operator<< (std::ostream& stream, const bufferQueueSyncMPSC& obj);

    public:
    bufferQueueSyncMPSC(size_t capacity): bufferQueue{capacity} {}

    bool push(const char* ptrIn, size_t len)
    {
        std::lock_guard<std::mutex> l{_mtx};
        return bufferQueue::push(ptrIn, len);
    }
    std::pair<const char*, size_t> front(std::string& buffer)
    {
        return bufferQueue::front(buffer);
    }
    bool pop()
    {
        return bufferQueue::pop();
    }

    private:
    std::mutex _mtx;
};
std::ostream& operator<< (std::ostream& stream, const bufferQueueSyncMPSC& obj)
{
    stream << static_cast<const bufferQueue&>(obj);
    return stream;
}

class bufferQueueSyncSPMC : protected bufferQueue
{
    friend std::ostream& operator<< (std::ostream& stream, const bufferQueueSyncSPMC& obj);

    public:
    bufferQueueSyncSPMC(size_t capacity): bufferQueue{capacity} {}

    bool push(const char* ptrIn, size_t len)
    {
        return bufferQueue::push(ptrIn, len);
    }
    std::pair<const char*, size_t> pop(std::string& buffer)
    {
        std::lock_guard<std::mutex> l{_mtx};
        auto [ptr, len] = bufferQueue::front(buffer);
        if (ptr == nullptr || len == 0)
        {
            return {nullptr, 0};
        }

        if (ptr != buffer.c_str())
        {
            buffer.resize(len);
            std::memcpy(buffer.data(), ptr, len);
        }
        bufferQueue::pop();
        return {buffer.data(), buffer.size()};
    }

    private:
    std::mutex _mtx;
};

std::ostream& operator<< (std::ostream& stream, const bufferQueueSyncSPMC& obj)
{
    stream << static_cast<const bufferQueue&>(obj);
    return stream;
}

class bufferQueueSyncMPMC : public bufferQueueSyncSPMC
{
    friend std::ostream& operator<< (std::ostream& stream, const bufferQueueSyncMPMC& obj);

    public:
    bufferQueueSyncMPMC(size_t capacity): bufferQueueSyncSPMC{capacity} {}

    bool push(const char* ptrIn, size_t len)
    {
        std::lock_guard<std::mutex> l{_mtx};
        return bufferQueue::push(ptrIn, len);
    }

    private:
    std::mutex _mtx;
};

std::ostream& operator<< (std::ostream& stream, const bufferQueueSyncMPMC& obj)
{
    stream << static_cast<const bufferQueue&>(obj);
    return stream;
}