#ifndef RESERVATION_SERVER_SERVER_HPP
#define RESERVATION_SERVER_SERVER_HPP

#include <atomic>
#include <memory>
#include <mutex>

#include "core/reservation_table.hpp"
#include "ipc/posix_message_queue.hpp"
#include "server/request_processor.hpp"
#include "server/server_config.hpp"
#include "server/worker_pool.hpp"

namespace css223::server {

class Server {
public:
    explicit Server(ServerConfig config);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    Server(Server&&) = delete;
    Server& operator=(Server&&) = delete;

    bool start();
    void stop() noexcept;
    void run();

    [[nodiscard]] bool is_running() const noexcept { return running_.load(); }
    [[nodiscard]] const ServerConfig& config() const noexcept { return config_; }
    [[nodiscard]] const core::ReservationTable& table() const noexcept { return table_; }

private:
    ServerConfig config_;
    core::ReservationTable table_;
    std::mutex reservation_mutex_;
    ipc::PosixMessageQueue request_queue_;
    RequestProcessor processor_;
    WorkerPool worker_pool_;
    std::atomic<bool> running_{false};
};

} // namespace css223::server

#endif // RESERVATION_SERVER_SERVER_HPP
