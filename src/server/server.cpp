#include "server/server.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

#include "ipc/message.hpp"
#include "ipc/posix_message_queue.hpp"

namespace css223::server {

namespace {

std::atomic<bool> g_shutdown_requested{false};
std::mutex g_worker_quit_log_mutex;

} // namespace

void Server::request_shutdown() noexcept {
    g_shutdown_requested.store(true, std::memory_order_relaxed);
}

bool Server::is_shutdown_requested() noexcept {
    return g_shutdown_requested.load(std::memory_order_relaxed);
}

void Server::reset_shutdown_request() noexcept {
    g_shutdown_requested.store(false, std::memory_order_relaxed);
}

Server::Server(ServerConfig config)
    : config_(std::move(config)), processor_(table_, reservation_mutex_, config_),
      worker_pool_(config_.worker_count) {}

Server::~Server() {
    stop();
}

bool Server::start() {
    if (running_.load()) {
        return true;
    }

    reset_shutdown_request();

    // Explicit queue lifecycle ownership: unlink stale queues before opening
    ipc::PosixMessageQueue::unlink(config_.request_queue_name);

    ipc::QueueConfig queue_config{};
    queue_config.max_messages = 10;
    queue_config.max_message_size = sizeof(ipc::RequestMessage);

    request_queue_ =
        ipc::PosixMessageQueue::open_or_create(config_.request_queue_name, queue_config);

    if (!request_queue_.is_open()) {
        return false;
    }

    running_.store(true);

    worker_pool_.start([this](std::size_t worker_id) {
        while (running_.load()) {
            ipc::RequestMessage request{};
            if (!request_queue_.receive(&request, sizeof(request))) {
                if (!running_.load()) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            if (!running_.load() || (request.command == common::CommandType::Quit &&
                                     request.client_id == common::kInvalidClientId)) {
                {
                    std::lock_guard<std::mutex> lock(g_worker_quit_log_mutex);
                    std::cout << "[Worker " << worker_id
                              << "] Received QUIT sentinel. Shutting down worker thread...\n";
                }
                break;
            }

            ipc::ResponseMessage response = processor_.process_request(request, worker_id);

            std::string_view reply_queue_name = ipc::buffer_to_string_view(
                request.reply_queue_name, sizeof(request.reply_queue_name));

            if (!reply_queue_name.empty()) {
                auto client_queue = ipc::PosixMessageQueue::open_write_only(reply_queue_name);
                if (client_queue.is_open()) {
                    client_queue.send(&response, sizeof(response));
                }
            }
        }
    });

    return true;
}

void Server::stop() noexcept {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    // Unblock worker threads waiting on blocking mq_receive()
    if (request_queue_.is_open()) {
        ipc::RequestMessage quit_request{};
        quit_request.command = common::CommandType::Quit;
        for (std::size_t i = 0; i < config_.worker_count; ++i) {
            request_queue_.send(&quit_request, sizeof(quit_request), 10);
        }
    }

    worker_pool_.stop();
    request_queue_.close();
    ipc::PosixMessageQueue::unlink(config_.request_queue_name);
}

void Server::run() {
    if (!start()) {
        return;
    }

    while (running_.load() && !is_shutdown_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (running_.load()) {
        stop();
    }
}

} // namespace css223::server
