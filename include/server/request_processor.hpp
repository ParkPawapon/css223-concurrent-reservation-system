#ifndef RESERVATION_SERVER_REQUEST_PROCESSOR_HPP
#define RESERVATION_SERVER_REQUEST_PROCESSOR_HPP

#include <mutex>

#include "concurrency/random_delay.hpp"
#include "core/reservation_table.hpp"
#include "ipc/message.hpp"
#include "server/server_config.hpp"

namespace css223::server {

class RequestProcessor {
public:
    RequestProcessor(core::ReservationTable& table,
                     std::mutex& reservation_mutex,
                     const ServerConfig& config) noexcept;

    [[nodiscard]] ipc::ResponseMessage process_request(const ipc::RequestMessage& request);

private:
    core::ReservationTable& table_;
    std::mutex& reservation_mutex_;
    const ServerConfig& config_;
    concurrency::RandomDelayGenerator delay_generator_;
};

} // namespace css223::server

#endif // RESERVATION_SERVER_REQUEST_PROCESSOR_HPP
