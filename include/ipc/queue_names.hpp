#ifndef RESERVATION_IPC_QUEUE_NAMES_HPP
#define RESERVATION_IPC_QUEUE_NAMES_HPP

#include <string>
#include <string_view>

#include "common/constants.hpp"
#include "common/types.hpp"

namespace css223::ipc {

[[nodiscard]] constexpr std::string_view get_server_request_queue_name() noexcept {
    return common::kDefaultServerQueueName;
}

[[nodiscard]] std::string format_client_reply_queue_name(common::ClientId client_id);

[[nodiscard]] bool is_valid_posix_queue_name(std::string_view queue_name) noexcept;

} // namespace css223::ipc

#endif // RESERVATION_IPC_QUEUE_NAMES_HPP
