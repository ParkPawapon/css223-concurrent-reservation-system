#include "server/request_processor.hpp"

#include <cstddef>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

#include "common/command.hpp"
#include "common/result.hpp"
#include "concurrency/random_delay.hpp"
#include "core/seat.hpp"
#include "ipc/message.hpp"

namespace css223::server {

RequestProcessor::RequestProcessor(core::ReservationTable& table,
                                   std::mutex& reservation_mutex,
                                   const ServerConfig& config) noexcept
    : table_(table), reservation_mutex_(reservation_mutex), config_(config),
      delay_generator_(config.delay_min_ms, config.delay_max_ms) {}

ipc::ResponseMessage RequestProcessor::process_request(const ipc::RequestMessage& request) {
    ipc::ResponseMessage response{};
    response.request_id = request.request_id;
    response.client_id = request.client_id;
    response.result = common::StatusCode::Failure;

    std::string_view seat_id = ipc::buffer_to_string_view(request.seat_id, sizeof(request.seat_id));
    ipc::copy_string_to_buffer(response.seat_id, sizeof(response.seat_id), seat_id);

    auto delay_action = [this]() {
        if (config_.random_delay_enabled) {
            concurrency::RandomDelayGenerator::execute_delay(config_.delay_min_ms,
                                                             config_.delay_max_ms);
        }
    };

    switch (request.command) {
        case common::CommandType::List: {
            std::vector<core::Seat> snapshot;
            if (config_.synchronization_enabled) {
                std::scoped_lock lock(reservation_mutex_);
                snapshot = table_.get_all_seats();
            } else {
                snapshot = table_.get_all_seats();
            }
            // Response formatting occurs outside the critical section
            response.result = common::StatusCode::Success;
            ipc::copy_string_to_buffer(
                response.message, sizeof(response.message), "Seat table listed successfully");
            break;
        }

        case common::CommandType::Status: {
            std::optional<core::Seat> snapshot;
            if (config_.synchronization_enabled) {
                std::scoped_lock lock(reservation_mutex_);
                snapshot = table_.get_seat(seat_id);
            } else {
                snapshot = table_.get_seat(seat_id);
            }

            if (snapshot.has_value()) {
                response.result = common::StatusCode::Success;
                response.status = snapshot->status();
                response.owner_client_id =
                    snapshot->owner_client_id().value_or(common::kInvalidClientId);
                ipc::copy_string_to_buffer(
                    response.message, sizeof(response.message), "Status retrieved");
            } else {
                ipc::copy_string_to_buffer(
                    response.message, sizeof(response.message), "Seat not found");
            }
            break;
        }

        case common::CommandType::Reserve: {
            bool success = false;
            if (config_.synchronization_enabled) {
                std::scoped_lock lock(reservation_mutex_);
                success = table_.reserve_seat(seat_id, request.client_id, delay_action);
            } else {
                success = table_.reserve_seat(seat_id, request.client_id, delay_action);
            }

            if (success) {
                response.result = common::StatusCode::Success;
                response.status = core::SeatStatus::Reserved;
                response.owner_client_id = request.client_id;
                ipc::copy_string_to_buffer(
                    response.message, sizeof(response.message), "Seat reserved successfully");
            } else {
                ipc::copy_string_to_buffer(
                    response.message, sizeof(response.message), "Reservation failed");
            }
            break;
        }

        case common::CommandType::Cancel: {
            bool success = false;
            if (config_.synchronization_enabled) {
                std::scoped_lock lock(reservation_mutex_);
                success = table_.cancel_seat(seat_id, request.client_id, delay_action);
            } else {
                success = table_.cancel_seat(seat_id, request.client_id, delay_action);
            }

            if (success) {
                response.result = common::StatusCode::Success;
                response.status = core::SeatStatus::Available;
                response.owner_client_id = common::kInvalidClientId;
                ipc::copy_string_to_buffer(response.message,
                                           sizeof(response.message),
                                           "Reservation cancelled successfully");
            } else {
                ipc::copy_string_to_buffer(
                    response.message, sizeof(response.message), "Cancellation failed");
            }
            break;
        }

        case common::CommandType::Quit: {
            response.result = common::StatusCode::Success;
            ipc::copy_string_to_buffer(
                response.message, sizeof(response.message), "Client session ended");
            break;
        }

        case common::CommandType::Unknown:
        default: {
            response.result = common::StatusCode::Failure;
            ipc::copy_string_to_buffer(
                response.message, sizeof(response.message), "Unknown command");
            break;
        }
    }

    return response;
}

} // namespace css223::server
