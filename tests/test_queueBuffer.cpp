#include "queueBuffer.h"

#include <iostream>
#include <string>
#include <string_view>
#include <chrono>
#include <set>
#include <vector>
#include <thread>
#include <array>
#include <functional>
#include <random>
#include <limits>

constexpr static size_t MaxSeqnos{1024 * 1024};
std::array<std::atomic<size_t>, MaxSeqnos> receivedSeqnos;
void initReceivedSeqnos()
{
    std::fill(receivedSeqnos.begin(), receivedSeqnos.end(), 0);
}
void updateReceivedSeqno(size_t seqno)
{
    const auto index{seqno % receivedSeqnos.size()};

    receivedSeqnos[index] += 1;
}
bool verifyReceivedSeqnoVal(size_t seqno, std::function<bool(size_t)> pred)
{
    if (seqno > receivedSeqnos.size() - 1)
    {
        throw std::runtime_error{std::string{"verify: seqno "} + std::to_string(seqno) + " is greater then the max " + std::to_string(receivedSeqnos.size())};
    }
    if (!pred(receivedSeqnos[seqno]))
    {
        std::cout << "verify: seqno: " << seqno << " mismatch : received: " << receivedSeqnos[seqno] << std::endl;
        return false;
    }
    return true;
}
void summarize()
{
    size_t prevVal{receivedSeqnos[0]};
    size_t cnt{0};
    for (size_t i = 0 ; i < receivedSeqnos.size() ; i++)
    {
        if (prevVal == receivedSeqnos[i])
        {
            cnt += 1;
        }
        else
        {
            std::cout << cnt << ':' << prevVal << std::endl;
            prevVal = receivedSeqnos[i];
            cnt = 1;
        }
    }
    std::cout << cnt << ':' << prevVal << std::endl;
}

void makeData(size_t seqno, std::string& data)
{
    data = std::to_string(seqno) + '_';
    const auto prefixLen{data.size()};
    const size_t addlen{((seqno + 1) % 1000) + 1};
    data.resize(prefixLen + addlen);
    for (size_t i = prefixLen ; i < data.size() ; i++)
    {
        data.data()[i] = 'A' + ((i - prefixLen) % 26);
    }
}

class randomAgent
{
    public:
    randomAgent(double probability, std::function<void()> action)
    :_probability{probability}, _action{action}
    {}

    void randomlyAct(double probability)
    {
        if (probability > _dist(_mt))
        {
            _action();
        }
    }

    void randomlyAct()
    {
        randomlyAct(_probability);
    }

    double _probability;
    std::function<void()> _action;
    std::mt19937 _mt{(std::random_device())()};
    std::uniform_real_distribution<double> _dist{0.0, 1.0};
};

bool testRandomAgent()
{
    for (double probability{0.0} ; probability <= 1.0 ; probability += 0.1)
    {
        const size_t max{10000};
        size_t cnt{0};
        randomAgent ra{probability, [&cnt](){cnt++;}};

        for (size_t i = 0 ; i < max ; i++)
        {
            ra.randomlyAct();
        }

        std::cout << "test with probability: " << probability << " : " << cnt << ':' << max << " -> " << static_cast<double>(cnt) / static_cast<double>(max) << std::endl;
    }
    return true;
}

bool testInterface()
{
	std::cout << __FUNCTION__ << " Test : interfaces " << std::endl;
	std::cout << "-------------------------------------------------" << std::endl;

	bufferQueue queue{1024};
    std::cout << queue << std::endl;

    size_t seqno{0};
    std::string data;
    do
    { 
        makeData(seqno++, data);
        std::cout << "pushing: " << data << std::endl;
    } 
    while (queue.push(data.c_str(), data.size()));
    
    std::cout << std::endl;

    std::string expected;
    while(!queue.empty())
    {
        auto [ptr, len] = queue.front(data);
        std::cout << "pulled: " << std::string_view{ptr, len} << std::endl;

        const std::string_view received{ptr, len};
        size_t receivedSeqno{0};//{std::atoi(ptr)};
        sscanf(ptr, "%zu", &receivedSeqno);
        makeData(receivedSeqno, expected);
        if (expected != received)
        {
            std::cout << __FILE__ << ':' << __LINE__ 
                      << "Error: data mismatch: received seqno: " << receivedSeqno << std::endl
                      << "expected: " << expected << std::endl
                      << "received: " << received << std::endl;
            std::terminate();
        }

        queue.pop();
    }
    

    return true;
}

template<typename QueueType>
bool testQueueMultiThread(size_t queueSize, size_t numProducers)
{
    QueueType queue{queueSize};
    std::cout << queue << std::endl;

    initReceivedSeqnos();

    std::atomic<bool> startTest{false};
    std::atomic<size_t> pusherFinished{0};
    auto pusher{[&startTest, &pusherFinished, &queue](size_t startSeqno, size_t endSeqno){
        std::cout << "pusher seqnos: [" << startSeqno << ", " << endSeqno << ')' << std::endl;
        while(!startTest){std::this_thread::yield();}

        randomAgent randomYield(0.001, [](){std::this_thread::yield();});
        std::string data;
        for(auto seqno = startSeqno ; seqno < endSeqno; seqno++)
        { 
            makeData(seqno, data);
            while (!queue.push(data.c_str(), data.size()));

            //std::cout << "pushed: seqno: " << seqno << ", len: " << data.size() << std::endl;
            if ((seqno - startSeqno) % 100'000 == 0)
            {
                std::cout << "pushed up to " << seqno << std::endl;
            }
            randomYield.randomlyAct();
        }

        std::cout << "pushed finished" << std::endl;
        pusherFinished += 1;
    }};
    
    auto puller{[&pusherFinished, numProducers, &queue](std::chrono::milliseconds timeout){
        randomAgent randomYield(0.001, [](){std::this_thread::yield();});
        std::string data, expected;
        const auto endTp{std::chrono::system_clock::now() + timeout};
        while (endTp > std::chrono::system_clock::now())
        {
            auto [ptr, len] = queue.front(data);
            if (ptr == nullptr)
            {
                if (pusherFinished == numProducers)
                {
                    std::cout << "all pushers finished, poller finished too" << std::endl;
                    return;
                }
                std::this_thread::yield();
                continue;
            }

            for ([[maybe_unused]]auto i : {0, 1})
            {
                auto [ptr, len] = queue.front(data);
                //assert(ptr == data.c_str());

                const std::string_view received{ptr, len};
                size_t receivedSeqno{0};
                sscanf(ptr, "%zu", &receivedSeqno);
                makeData(receivedSeqno, expected);
                if (expected != received)
                {
                    std::cout << __FILE__ << ':' << __LINE__ 
                              << " - Error: data mismatch: received seqno: " << receivedSeqno << std::endl
                              << "expected: " << expected << std::endl
                              << "received: " << received << std::endl;
                    queue.pop();
                    std::terminate();
                }
                updateReceivedSeqno(receivedSeqno);

                if (i == 0 && (receivedSeqno) % 1'000'000 == 0)
                {
                    std::cout << "pulled up to " << receivedSeqno << std::endl;
                }
            }
            queue.pop();
            randomYield.randomlyAct();
        }
    }};

    std::vector<std::thread> threads;
    threads.emplace_back(puller, std::chrono::milliseconds{1000000000});
    
    const size_t startSeqno{0};
    const size_t endSeqno{MaxSeqnos * 10};// * 1024 * 10};
    const size_t batchSize{(endSeqno - startSeqno) / numProducers};
    for (size_t i = 0 ; i < numProducers ; i++)
    {
        threads.emplace_back(pusher, i * batchSize, i * batchSize + batchSize);
    }

    std::this_thread::sleep_for(std::chrono::seconds{1});
    startTest = true;

    for(auto& t : threads)
    {
        t.join();
    }

    const auto expectedVal{static_cast<size_t>(std::ceil((endSeqno - startSeqno) / MaxSeqnos)) * 2};
    auto predicate{[expectedVal](size_t val){return expectedVal == val;}};
    for(size_t seqno = 0 ; seqno < numProducers * batchSize; seqno++)
    {
        if (!verifyReceivedSeqnoVal(seqno % MaxSeqnos, predicate))
        {
           std::cout << __FILE__ << ':' << __LINE__
                     << " - Error: data mismatch: seqno: " << seqno << " was not received properly, expected: " << expectedVal << std::endl;
        }
    }

    return true;
}

template<typename QueueType>
bool testQueueMultiConsumersThread(size_t queueSize, size_t numProducers, size_t numConsumers)
{
    QueueType queue{queueSize};
    std::cout << queue << std::endl;

    initReceivedSeqnos();

    std::atomic<bool> startTest{false};
    std::atomic<size_t> pusherFinished{0};
    auto pusher{[&startTest, &pusherFinished, &queue](size_t startSeqno, size_t endSeqno){
        std::cout << "pusher seqnos: [" << startSeqno << ", " << endSeqno << ')' << std::endl;
        while(!startTest){std::this_thread::yield();}

        randomAgent randomYield(0.001, [](){std::this_thread::yield();});
        std::string data;
        for(auto seqno = startSeqno ; seqno < endSeqno; seqno++)
        { 
            makeData(seqno, data);
            while (!queue.push(data.c_str(), data.size()));

            //std::cout << "pushed: seqno: " << seqno << ", len: " << data.size() << std::endl;
            if ((seqno - startSeqno) % 1'000'000 == 0)
            {
                std::cout << "pushed up to " << seqno << std::endl;
            }
            randomYield.randomlyAct();
        }

        std::cout << "pushed finished" << std::endl;
        pusherFinished += 1;
    }};
    
    auto puller{[&pusherFinished, numProducers, &queue](std::chrono::milliseconds timeout){
        randomAgent randomYield(0.001, [](){std::this_thread::yield();});
        std::string data, expected;
        const auto endTp{std::chrono::system_clock::now() + timeout};
        while (endTp > std::chrono::system_clock::now())
        {
            auto [ptr, len] = queue.pop(data);
            if (ptr == nullptr)
            {
                if (pusherFinished == numProducers)
                {
                    std::cout << "all pushers finished, poller finished too" << std::endl;
                    return;
                }
                std::this_thread::yield();
                continue;
            }
            assert(ptr == data.c_str());

            const std::string_view received{ptr, len};
            size_t receivedSeqno{0};
            sscanf(ptr, "%zu", &receivedSeqno);
            makeData(receivedSeqno, expected);
            if (expected != received)
            {
                std::cout << __FILE__ << ':' << __LINE__ 
                          << " - Error: data mismatch: received seqno: " << receivedSeqno << std::endl
                          << "expected: " << expected << std::endl
                          << "received: " << received << std::endl;
                std::terminate();
            }
            updateReceivedSeqno(receivedSeqno);

            if ((receivedSeqno) % 1'000'000 == 0)
            {
                std::cout << "pulled up to " << receivedSeqno << std::endl;
            }

            randomYield.randomlyAct();
        }
    }};

    std::vector<std::thread> threads;
    for (size_t i = 0 ; i < numConsumers ; i++)
    {
        threads.emplace_back(puller, std::chrono::milliseconds{1000000000});
    }
    
    const size_t startSeqno{0};
    const size_t endSeqno{MaxSeqnos * 10};
    const size_t batchSize{(endSeqno - startSeqno) / numProducers};
    for (size_t i = 0 ; i < numProducers ; i++)
    {
        threads.emplace_back(pusher, i * batchSize, i * batchSize + batchSize);
    }

    std::this_thread::sleep_for(std::chrono::seconds{1});
    startTest = true;

    for(auto& t : threads)
    {
        t.join();
    }

    summarize();

    const auto expectedVal{static_cast<size_t>(std::ceil((endSeqno - startSeqno) / MaxSeqnos))};
    auto predicate{[expectedVal](size_t val){return expectedVal == val;}};
    for(size_t seqno = startSeqno ; seqno < numProducers * batchSize; seqno++)
    {
        if (!verifyReceivedSeqnoVal(seqno % MaxSeqnos, predicate))
        {
           std::cout << __FILE__ << ':' << __LINE__
                     << " - Error: data mismatch: seqno: " << seqno << " was not received properly, expected: " << expectedVal << std::endl;
        }
    }

    return true;
}

int main(int /*argc*/, char* /*argv*/[])
{
	if (!testInterface())
		return __LINE__;
    if (!testRandomAgent())
        return __LINE__;
    if (!testQueueMultiThread<bufferQueueSyncSPSC>(1024, 1))
		return __LINE__;
    if (!testQueueMultiThread<bufferQueueSyncMPSC>(1024, 5))
		return __LINE__;
    if (!testQueueMultiConsumersThread<bufferQueueSyncSPMC>(1024, 1, 5))
		return __LINE__;
    if (!testQueueMultiConsumersThread<bufferQueueSyncMPMC>(1024, 5, 5))
		return __LINE__;
	return 0;
}