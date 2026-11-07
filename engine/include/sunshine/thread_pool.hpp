#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <future>

namespace Sunshine {

class WorkerThreadPool {
public:
    explicit WorkerThreadPool(size_t threadCount = 0);
    ~WorkerThreadPool();

    WorkerThreadPool(const WorkerThreadPool&) = delete;
    WorkerThreadPool& operator=(const WorkerThreadPool&) = delete;

    size_t GetThreadCount() const noexcept { return m_threads.size(); }

    void Enqueue(std::function<void(size_t threadId)> task);

    void ParallelFor(size_t count, std::function<void(size_t index, size_t threadId)> func);

    void WaitAll();

private:
    void WorkerLoop(size_t threadId);

    std::vector<std::thread> m_threads;
    std::queue<std::function<void(size_t threadId)>> m_tasks;
    std::mutex m_queueMutex;
    std::condition_variable m_cv;
    std::condition_variable m_waitCv;
    std::atomic<bool> m_stop{false};
    std::atomic<int32_t> m_activeTasks{0};
};

} // namespace Sunshine
