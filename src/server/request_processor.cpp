#include "server/request_processor.hpp"

#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "common/command.hpp"
#include "common/result.hpp"
#include "concurrency/random_delay.hpp"
#include "core/seat.hpp"
#include "ipc/message.hpp"

namespace css223::server {

namespace {

std::mutex g_log_mutex;

void log_worker(std::size_t worker_id, std::string_view step, std::string_view message) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::cout << "[Worker " << worker_id << "] [" << step << "] " << message << "\n";
    std::cout.flush();
}

} // namespace

RequestProcessor::RequestProcessor(core::ReservationTable& table,
                                   std::mutex& reservation_mutex,
                                   const ServerConfig& config) noexcept
    : table_(table), reservation_mutex_(reservation_mutex), config_(config),
      delay_generator_(config.delay_min_ms, config.delay_max_ms) {}

ipc::ResponseMessage RequestProcessor::process_request(const ipc::RequestMessage& request,
                                                       std::size_t worker_id) {
    ipc::ResponseMessage response{};
    response.request_id = request.request_id;
    response.client_id = request.client_id;
    response.result = common::StatusCode::Failure;

    std::string_view seat_id = ipc::buffer_to_string_view(request.seat_id, sizeof(request.seat_id));
    ipc::copy_string_to_buffer(response.seat_id, sizeof(response.seat_id), seat_id);

    switch (request.command) {
        case common::CommandType::List: {
            log_worker(worker_id,
                       "LIST",
                       "Processing LIST request for Client " + std::to_string(request.client_id));
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
            log_worker(worker_id,
                       "STATUS",
                       "Checking status of Seat " + std::string(seat_id) + " for Client " +
                           std::to_string(request.client_id));
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
            bool was_available = false;

            auto reserve_op = [&]() {
                // Step 1: CHECK
                auto current_seat = table_.get_seat(seat_id);
                if (!current_seat.has_value()) {
                    log_worker(worker_id, "CHECK", "Seat " + std::string(seat_id) + " not found");
                    return false;
                }

                was_available = current_seat->is_available();
                if (was_available) {
                    log_worker(worker_id,
                               "CHECK",
                               "Seat " + std::string(seat_id) + " is AVAILABLE for Client " +
                                   std::to_string(request.client_id));
                } else {
                    std::string owner_str = current_seat->owner_client_id().has_value()
                                                ? std::to_string(*current_seat->owner_client_id())
                                                : "unknown";
                    log_worker(worker_id,
                               "CHECK",
                               "Seat " + std::string(seat_id) +
                                   " is ALREADY RESERVED (Owner: Client " + owner_str + ")");
                }

                // Step 2 & 3: DELAY and UPDATE inside table_.reserve_seat
                auto delay_action = [&]() {
                    if (config_.random_delay_enabled) {
                        unsigned int delay_ms = concurrency::RandomDelayGenerator::execute_delay(
                            config_.delay_min_ms, config_.delay_max_ms);
                        log_worker(worker_id,
                                   "DELAY",
                                   "Simulating random delay: " + std::to_string(delay_ms) +
                                       " ms for Seat " + std::string(seat_id));
                    }
                };

                return table_.reserve_seat(seat_id, request.client_id, delay_action);
            };

            if (config_.synchronization_enabled) {
                std::scoped_lock lock(reservation_mutex_);
                success = reserve_op();
            } else {
                success = reserve_op();
            }

            // Step 3: UPDATE logging
            if (success) {
                log_worker(worker_id,
                           "UPDATE",
                           "Seat " + std::string(seat_id) + " SUCCESS: Reserved by Client " +
                               std::to_string(request.client_id));
                response.result = common::StatusCode::Success;
                response.status = core::SeatStatus::Reserved;
                response.owner_client_id = request.client_id;
                ipc::copy_string_to_buffer(
                    response.message, sizeof(response.message), "Seat reserved successfully");
            } else {
                if (was_available) {
                    log_worker(worker_id,
                               "UPDATE",
                               "CONFLICT / DOUBLE BOOKING: Seat " + std::string(seat_id) +
                                   " reservation FAILED for Client " +
                                   std::to_string(request.client_id) +
                                   " (Seat was reserved by another thread during delay!)");
                } else {
                    log_worker(worker_id,
                               "UPDATE",
                               "Seat " + std::string(seat_id) + " reservation FAILED for Client " +
                                   std::to_string(request.client_id));
                }
                ipc::copy_string_to_buffer(
                    response.message, sizeof(response.message), "Reservation failed");
            }
            break;
        }

        case common::CommandType::Cancel: {
            bool success = false;
            bool was_owned = false;

            auto cancel_op = [&]() {
                // Step 1: CHECK
                auto current_seat = table_.get_seat(seat_id);
                if (!current_seat.has_value()) {
                    log_worker(worker_id, "CHECK", "Seat " + std::string(seat_id) + " not found");
                    return false;
                }

                was_owned = current_seat->is_reserved() &&
                            current_seat->owner_client_id() == request.client_id;
                if (was_owned) {
                    log_worker(worker_id,
                               "CHECK",
                               "Seat " + std::string(seat_id) + " is RESERVED by Client " +
                                   std::to_string(request.client_id) + " (Ownership verified)");
                } else {
                    log_worker(worker_id,
                               "CHECK",
                               "Seat " + std::string(seat_id) + " cancellation DENIED: Client " +
                                   std::to_string(request.client_id) + " is not the owner");
                }

                // Step 2 & 3: DELAY and UPDATE inside table_.cancel_seat
                auto delay_action = [&]() {
                    if (config_.random_delay_enabled) {
                        unsigned int delay_ms = concurrency::RandomDelayGenerator::execute_delay(
                            config_.delay_min_ms, config_.delay_max_ms);
                        log_worker(worker_id,
                                   "DELAY",
                                   "Simulating random delay: " + std::to_string(delay_ms) +
                                       " ms for Seat " + std::string(seat_id));
                    }
                };

                return table_.cancel_seat(seat_id, request.client_id, delay_action);
            };

            if (config_.synchronization_enabled) {
                std::scoped_lock lock(reservation_mutex_);
                success = cancel_op();
            } else {
                success = cancel_op();
            }

            // Step 3: UPDATE logging
            if (success) {
                log_worker(worker_id,
                           "UPDATE",
                           "Seat " + std::string(seat_id) + " SUCCESS: Cancelled by Client " +
                               std::to_string(request.client_id));
                response.result = common::StatusCode::Success;
                response.status = core::SeatStatus::Available;
                response.owner_client_id = common::kInvalidClientId;
                ipc::copy_string_to_buffer(response.message,
                                           sizeof(response.message),
                                           "Reservation cancelled successfully");
            } else {
                log_worker(worker_id,
                           "UPDATE",
                           "Seat " + std::string(seat_id) + " cancellation FAILED for Client " +
                               std::to_string(request.client_id));
                ipc::copy_string_to_buffer(
                    response.message, sizeof(response.message), "Cancellation failed");
            }
            break;
        }

        case common::CommandType::Quit: {
            log_worker(worker_id,
                       "QUIT",
                       "Client " + std::to_string(request.client_id) + " session ended");
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
