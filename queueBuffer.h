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
        const auto [blocksAhead, blocksOverlap] = toWriteBlocks(_head.load(), _tail.load(), _capacityBlocks);
        const auto blocksNeeded{numOfBlocks(len + sizeof(header))};
        if (blocksAhead + blocksOverlap < blocksNeeded)
        {
            return false;
        }

        auto* ptr{_buffer + _head * BlockSize};
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

        const auto headVal{_head.load()};
        _head = (headVal + blocksNeeded) % _capacityBlocks;
#if 0
        std::cout << "head: " << headVal << " -> " << _head
                  << ", tail: " << _tail
                  << ", blocks: " << _capacityBlocks
                  << std::endl;
#endif        
        return true;
    }
    std::pair<const char*, size_t> front(std::string& buffer)
    {
        if (empty())
        {
            return {nullptr, 0};
        }

        const auto* ptr{_buffer + _tail * BlockSize};
        const auto* headerPtr{reinterpret_cast<const header*>(ptr)};
        if (!headerPtr->verifyMagic())
        {
            assert(false && "corrupted data, failed to verify magic");
            
        }
        
        ptr += sizeof(header);

        const auto [blocksAhead, blocksOverlap] = toReadBlocks(_head.load(), _tail.load(), _capacityBlocks);
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
        //std::cout << "front overlap: bufferOverlapLen: " << bufferOverlapLen << std::endl;

        assert(bufferOverlapLen + bufferAheadLen == headerPtr->_len);

        return {buffer.c_str(), buffer.size()};
    }
    bool pop()
    {
        if (empty())
        {
            return false;
        }

        const auto* ptr{_buffer + _tail * BlockSize};
        const auto* headerPtr{reinterpret_cast<const header*>(ptr)};
        assert(headerPtr->verifyMagic());

        const auto blocksToSkip{numOfBlocks(headerPtr->_len + sizeof(header))};

        auto tailVal{_tail.load()};
        _tail = (tailVal + blocksToSkip) % _capacityBlocks;

#if 0
        std::cout << "head: " << _head
                  << ", tail: " << tailVal << " -> " << _tail
                  << ", blocksToSkip: " << blocksToSkip
                  << ", blocks: " << _capacityBlocks
                  << std::endl;
#endif
        return true;
    }

    bool empty() const noexcept
    {
        return _head == _tail;
    }
    bool full() const noexcept
    {
        return (_head + 1) % _capacityBlocks == _tail;
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

class bufferQueueSyncMPSC : private bufferQueue
{
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

class bufferQueueSyncSPMC : private bufferQueue
{
    public:
    bufferQueueSyncSPMC(size_t capacity): bufferQueue{capacity} {}

    bool push(const char* ptrIn, size_t len)
    {
        return bufferQueue::push(ptrIn, len);
    }
    std::pair<const char*, size_t> front(std::string& buffer)
    {
        std::lock_guard<std::mutex> l{_mtx};
        auto [ptr, len] = bufferQueue::front(buffer);
        if (ptr == nullptr || len == 0)
        {
            return {ptr, len};
        }

        if (ptr != buffer.c_str())
        {
            buffer.resize(len);
            std::memcpy(buffer.data(), ptr, len);
            return {buffer.data(), len};
        }
        return {ptr, len}; 
    }
    bool pop()
    {
        std::lock_guard<std::mutex> l{_mtx};
        return bufferQueue::pop();
    }

    private:
    std::mutex _mtx;
};

class bufferQueueSyncMPMC : private bufferQueue
{
    public:
    bufferQueueSyncMPMC(size_t capacity): bufferQueue{capacity} {}

    bool push(const char* ptrIn, size_t len)
    {
        std::lock_guard<std::mutex> l{_mtx};
        return bufferQueue::push(ptrIn, len);
    }
    std::pair<const char*, size_t> front(std::string& buffer)
    {
        std::lock_guard<std::mutex> l{_mtx};
        auto [ptr, len] = bufferQueue::front(buffer);
        if (ptr == nullptr || len == 0)
        {
            return {ptr, len};
        }

        if (ptr != buffer.c_str())
        {
            buffer.resize(len);
            std::memcpy(buffer.data(), ptr, len);
            return {buffer.data(), len};
        }
        return {ptr, len}; 
    }
    bool pop()
    {
        std::lock_guard<std::mutex> l{_mtx};
        return bufferQueue::pop();
    }

    private:
    std::mutex _mtx;
};