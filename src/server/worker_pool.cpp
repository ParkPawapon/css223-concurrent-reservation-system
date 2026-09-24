#include "server/worker_pool.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <thread>
#include <vector>

#include "common/constants.hpp"

namespace css223::server {

WorkerPool::WorkerPool(std::size_t worker_count)
    : worker_count_(std::max(worker_count, common::kMinWorkerCount)) {}

WorkerPool::~WorkerPool() {
    stop();
}

void WorkerPool::start(const Task& task) {
    if (running_.load()) {
        return;
    }

    running_.store(true);
    workers_.reserve(worker_count_);

    for (std::size_t i = 0; i < worker_count_; ++i) {
        workers_.emplace_back([task, worker_id = i]() {
            if (task) {
                task(worker_id);
            }
        });
    }
}

void WorkerPool::stop() noexcept {
    running_.store(false);

    for (auto& worker_thread : workers_) {
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }
    workers_.clear();
}

} // namespace css223::server
