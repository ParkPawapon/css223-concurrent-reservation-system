#ifndef RESERVATION_SERVER_WORKER_POOL_HPP
#define RESERVATION_SERVER_WORKER_POOL_HPP

#include <atomic>
#include <cstddef>
#include <functional>
#include <thread>
#include <vector>

namespace css223::server {

class WorkerPool {
public:
    using Task = std::function<void(std::size_t worker_id)>;

    explicit WorkerPool(std::size_t worker_count);
    ~WorkerPool();

    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    WorkerPool(WorkerPool&&) = delete;
    WorkerPool& operator=(WorkerPool&&) = delete;

    void start(const Task& task);
    void stop() noexcept;

    [[nodiscard]] std::size_t worker_count() const noexcept { return worker_count_; }
    [[nodiscard]] bool is_running() const noexcept { return running_.load(); }

private:
    std::size_t worker_count_;
    std::atomic<bool> running_{false};
    std::vector<std::thread> workers_;
};

} // namespace css223::server

#endif // RESERVATION_SERVER_WORKER_POOL_HPP
