#ifndef RESERVATION_SERVER_SERVER_CONFIG_HPP
#define RESERVATION_SERVER_SERVER_CONFIG_HPP

#include <cstddef>
#include <string>

#include "common/constants.hpp"

namespace css223::server {

struct ServerConfig {
    std::size_t worker_count{common::kDefaultWorkerCount};
    bool synchronization_enabled{true};
    bool random_delay_enabled{false};
    unsigned int delay_min_ms{common::kDefaultMinDelayMs};
    unsigned int delay_max_ms{common::kDefaultMaxDelayMs};
    std::string request_queue_name{common::kDefaultServerQueueName};
};

} // namespace css223::server

#endif // RESERVATION_SERVER_SERVER_CONFIG_HPP
