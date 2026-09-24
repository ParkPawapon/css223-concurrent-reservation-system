#ifndef RESERVATION_IPC_MESSAGE_HPP
#define RESERVATION_IPC_MESSAGE_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include "common/command.hpp"
#include "common/constants.hpp"
#include "common/result.hpp"
#include "common/types.hpp"
#include "core/seat.hpp"

namespace css223::ipc {

struct RequestMessage {
    common::RequestId request_id{0};
    common::ClientId client_id{0};
    common::CommandType command{common::CommandType::Unknown};
    char seat_id[common::kMaxSeatIdLength]{0};
    char reply_queue_name[common::kMaxQueueNameLength]{0};
};

struct ResponseMessage {
    common::RequestId request_id{0};
    common::ClientId client_id{0};
    common::StatusCode result{common::StatusCode::Failure};
    char seat_id[common::kMaxSeatIdLength]{0};
    core::SeatStatus status{core::SeatStatus::Available};
    common::ClientId owner_client_id{0};
    char message[common::kMaxMessageTextLength]{0};
};

static_assert(std::is_trivially_copyable_v<RequestMessage>,
              "RequestMessage must be trivially copyable for raw POSIX MQ transfer");
static_assert(std::is_trivially_copyable_v<ResponseMessage>,
              "ResponseMessage must be trivially copyable for raw POSIX MQ transfer");

void copy_string_to_buffer(char* dest, std::size_t dest_size, std::string_view src) noexcept;

[[nodiscard]] std::string_view buffer_to_string_view(const char* src,
                                                     std::size_t max_size) noexcept;

} // namespace css223::ipc

#endif // RESERVATION_IPC_MESSAGE_HPP
