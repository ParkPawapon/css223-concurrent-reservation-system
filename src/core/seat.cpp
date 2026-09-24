#include "core/seat.hpp"

#include <utility>

namespace css223::core {

Seat::Seat(std::string seat_id) : id_(std::move(seat_id)) {}

bool Seat::reserve(common::ClientId client_id) {
    if (status_ == SeatStatus::Reserved || client_id == common::kInvalidClientId) {
        return false;
    }
    status_ = SeatStatus::Reserved;
    owner_client_id_ = client_id;
    return true;
}

bool Seat::cancel(common::ClientId client_id) {
    if (status_ != SeatStatus::Reserved || owner_client_id_ != client_id) {
        return false;
    }
    status_ = SeatStatus::Available;
    owner_client_id_ = std::nullopt;
    return true;
}

void Seat::reset() noexcept {
    status_ = SeatStatus::Available;
    owner_client_id_ = std::nullopt;
}

} // namespace css223::core
