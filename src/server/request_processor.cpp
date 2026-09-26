#include "server/request_processor.hpp"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "common/command.hpp"
#include "common/constants.hpp"
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

void RequestProcessor::log(std::size_t worker_id, std::string_view message) const {
    std::scoped_lock lock(log_mutex_);
    std::cout << "[Worker " << worker_id << "] " << message << "\n";
    std::cout.flush();
}

ipc::ResponseMessage RequestProcessor::process_request(const ipc::RequestMessage& request,
                                                       std::size_t worker_id) {
    ipc::ResponseMessage response{};
    response.request_id = request.request_id;
    response.client_id = request.client_id;
    response.result = common::StatusCode::Failure;

    std::string_view seat_id = ipc::buffer_to_string_view(request.seat_id, sizeof(request.seat_id));
    ipc::copy_string_to_buffer(response.seat_id, sizeof(response.seat_id), seat_id);

    auto delay_action = [this, worker_id, seat_id]() {
        if (config_.random_delay_enabled) {
            unsigned int delay_ms = delay_generator_.generate_delay_duration();
            log(worker_id,
                "Simulating delay: " + std::to_string(delay_ms) + " ms for seat " +
                    std::string(seat_id) + "...");
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
    };

    switch (request.command) {
        case common::CommandType::List: {
            log(worker_id,
                "Received LIST request from Client " + std::to_string(request.client_id));
            std::vector<core::Seat> snapshot;
            if (config_.synchronization_enabled) {
                std::scoped_lock lock(reservation_mutex_);
                snapshot = table_.get_all_seats();
            } else {
                snapshot = table_.get_all_seats();
            }
            response.result = common::StatusCode::Success;
            ipc::copy_string_to_buffer(
                response.message, sizeof(response.message), "Seat table listed successfully");
            log(worker_id,
                "Returned seat table snapshot (" + std::to_string(snapshot.size()) +
                    " seats) to Client " + std::to_string(request.client_id));
            break;
        }

        case common::CommandType::Status: {
            log(worker_id,
                "Received STATUS request for seat " + std::string(seat_id) + " from Client " +
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
                std::string status_str =
                    snapshot->is_available()
                        ? "AVAILABLE"
                        : "RESERVED by Client " + std::to_string(response.owner_client_id);
                log(worker_id, "Seat " + std::string(seat_id) + " status is " + status_str);
            } else {
                ipc::copy_string_to_buffer(
                    response.message, sizeof(response.message), "Seat not found");
                log(worker_id, "Seat " + std::string(seat_id) + " not found");
            }
            break;
        }

        case common::CommandType::Reserve: {
            log(worker_id,
                "Received RESERVE request for seat " + std::string(seat_id) + " from Client " +
                    std::to_string(request.client_id));

            bool success = false;
            if (config_.synchronization_enabled) {
                // Synchronized Mode: Mutex covers CHECK -> DELAY -> UPDATE
                std::unique_lock<std::mutex> lock(reservation_mutex_);

                // 1. CHECK
                auto seat_opt = table_.get_seat(seat_id);
                if (!seat_opt.has_value() || !seat_opt->is_available()) {
                    common::ClientId owner_id =
                        seat_opt.has_value()
                            ? seat_opt->owner_client_id().value_or(common::kInvalidClientId)
                            : common::kInvalidClientId;
                    std::string owner_str = (owner_id != common::kInvalidClientId)
                                                ? std::to_string(owner_id)
                                                : "another client";
                    log(worker_id,
                        "Checking seat " + std::string(seat_id) +
                            ": OCCUPIED (already reserved by Client " + owner_str +
                            "). Reservation rejected.");
                    success = false;
                } else {
                    log(worker_id, "Checking seat " + std::string(seat_id) + ": AVAILABLE");

                    // 2. DELAY
                    delay_action();

                    // 3. UPDATE
                    success = table_.reserve_seat(seat_id, request.client_id, nullptr, false);
                    if (success) {
                        log(worker_id,
                            "Updated seat " + std::string(seat_id) +
                                " status: SUCCESS (Reserved by Client " +
                                std::to_string(request.client_id) + ")");
                    } else {
                        log(worker_id,
                            "Updated seat " + std::string(seat_id) +
                                " status: FAILED (Seat was taken)");
                    }
                }
            } else {
                // Unsynchronized Mode (--no-sync): NO MUTEX covers CHECK -> DELAY -> UPDATE
                // 1. CHECK
                auto seat_opt = table_.get_seat(seat_id);
                if (!seat_opt.has_value()) {
                    log(worker_id, "Checking seat " + std::string(seat_id) + ": NOT FOUND");
                    success = false;
                } else if (!seat_opt->is_available()) {
                    common::ClientId owner_id =
                        seat_opt->owner_client_id().value_or(common::kInvalidClientId);
                    std::string owner_str = (owner_id != common::kInvalidClientId)
                                                ? std::to_string(owner_id)
                                                : "unknown";
                    log(worker_id,
                        "Checking seat " + std::string(seat_id) +
                            ": OCCUPIED (already reserved by Client " + owner_str +
                            "). Reservation rejected.");
                    success = false;
                } else {
                    log(worker_id, "Checking seat " + std::string(seat_id) + ": AVAILABLE");

                    // 2. DELAY (creates window for race condition / double booking)
                    delay_action();

                    // 3. UPDATE (unsynchronized write, allows overwrite)
                    success = table_.reserve_seat(seat_id, request.client_id, nullptr, true);
                    if (success) {
                        log(worker_id,
                            "Updated seat " + std::string(seat_id) +
                                " status: SUCCESS (Reserved by Client " +
                                std::to_string(request.client_id) + ")");
                    } else {
                        log(worker_id, "Updated seat " + std::string(seat_id) + " status: FAILED");
                    }
                }
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
            log(worker_id,
                "Received CANCEL request for seat " + std::string(seat_id) + " from Client " +
                    std::to_string(request.client_id));
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
                log(worker_id,
                    "Updated seat " + std::string(seat_id) +
                        " status: CANCELLED (Seat is now AVAILABLE)");
            } else {
                ipc::copy_string_to_buffer(
                    response.message, sizeof(response.message), "Cancellation failed");
                log(worker_id,
                    "Cancellation failed for seat " + std::string(seat_id) + " by Client " +
                        std::to_string(request.client_id));
            }
            break;
        }

        case common::CommandType::Quit: {
            log(worker_id,
                "Received QUIT request from Client " + std::to_string(request.client_id));
            response.result = common::StatusCode::Success;
            ipc::copy_string_to_buffer(
                response.message, sizeof(response.message), "Client session ended");
            break;
        }

        case common::CommandType::Unknown:
        default: {
            log(worker_id,
                "Received UNKNOWN request from Client " + std::to_string(request.client_id));
            response.result = common::StatusCode::Failure;
            ipc::copy_string_to_buffer(
                response.message, sizeof(response.message), "Unknown command");
            break;
        }
    }

    return response;
}

} // namespace css223::server
