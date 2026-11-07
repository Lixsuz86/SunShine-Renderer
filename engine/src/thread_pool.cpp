#include "sunshine/thread_pool.hpp"
#include <algorithm>

namespace Sunshine {

WorkerThreadPool::WorkerThreadPool(size_t threadCount) {
    if (threadCount == 0) {
        threadCount = std::max(1u, std::thread::hardware_concurrency());
    }

    m_threads.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i) {
        m_threads.emplace_back(&WorkerThreadPool::WorkerLoop, this, i);
    }
}

WorkerThreadPool::~WorkerThreadPool() {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_stop = true;
    }
    m_cv.notify_all();

    for (auto& t : m_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void WorkerThreadPool::Enqueue(std::function<void(size_t threadId)> task) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_tasks.push(std::move(task));
        m_activeTasks++;
    }
    m_cv.notify_one();
}

void WorkerThreadPool::ParallelFor(size_t count, std::function<void(size_t index, size_t threadId)> func) {
    if (count == 0) return;

    size_t numThreads = m_threads.size();
    if (numThreads <= 1 || count == 1) {
        for (size_t i = 0; i < count; ++i) {
            func(i, 0);
        }
        return;
    }

    size_t numBatches = std::min(count, numThreads);
    size_t itemsPerBatch = (count + numBatches - 1) / numBatches;

    for (size_t b = 0; b < numBatches; ++b) {
        size_t start = b * itemsPerBatch;
        size_t end = std::min(start + itemsPerBatch, count);

        if (start < end) {
            Enqueue([start, end, &func](size_t threadId) {
                for (size_t i = start; i < end; ++i) {
                    func(i, threadId);
                }
            });
        }
    }

    WaitAll();
}

void WorkerThreadPool::WaitAll() {
    std::unique_lock<std::mutex> lock(m_queueMutex);
    m_waitCv.wait(lock, [this] {
        return m_tasks.empty() && m_activeTasks == 0;
    });
}

void WorkerThreadPool::WorkerLoop(size_t threadId) {
    while (true) {
        std::function<void(size_t threadId)> task;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_cv.wait(lock, [this] {
                return m_stop || !m_tasks.empty();
            });

            if (m_stop && m_tasks.empty()) {
                return;
            }

            task = std::move(m_tasks.front());
            m_tasks.pop();
        }

        task(threadId);

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_activeTasks--;
            if (m_tasks.empty() && m_activeTasks == 0) {
                m_waitCv.notify_all();
            }
        }
    }
}

} // namespace Sunshine
