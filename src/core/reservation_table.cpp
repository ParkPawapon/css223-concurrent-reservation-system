#include "core/reservation_table.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "common/constants.hpp"

namespace css223::core {

ReservationTable::ReservationTable() {
    seats_.reserve(common::kCanonicalSeatIds.size());
    for (const auto& seat_id : common::kCanonicalSeatIds) {
        seats_.emplace_back(std::string(seat_id));
    }
}

std::size_t ReservationTable::seat_count() const noexcept {
    return seats_.size();
}

bool ReservationTable::has_seat(std::string_view seat_id) const noexcept {
    return find_seat_internal(seat_id) != nullptr;
}

std::optional<Seat> ReservationTable::get_seat(std::string_view seat_id) const {
    const Seat* seat = find_seat_internal(seat_id);
    if (seat == nullptr) {
        return std::nullopt;
    }
    return *seat;
}

std::vector<Seat> ReservationTable::get_all_seats() const {
    return seats_;
}

bool ReservationTable::reserve_seat(std::string_view seat_id,
                                    common::ClientId client_id,
                                    const DelayAction& delay_action) {
    Seat* seat = find_seat_internal(seat_id);
    if (seat == nullptr || !seat->is_available()) {
        return false;
    }

    if (delay_action) {
        delay_action();
    }

    return seat->reserve(client_id);
}

bool ReservationTable::cancel_seat(std::string_view seat_id,
                                   common::ClientId client_id,
                                   const DelayAction& delay_action) {
    Seat* seat = find_seat_internal(seat_id);
    if (seat == nullptr || !seat->is_reserved() || seat->owner_client_id() != client_id) {
        return false;
    }

    if (delay_action) {
        delay_action();
    }

    return seat->cancel(client_id);
}

void ReservationTable::reset_all() noexcept {
    for (auto& seat : seats_) {
        seat.reset();
    }
}

Seat* ReservationTable::find_seat_internal(std::string_view seat_id) noexcept {
    for (auto& seat : seats_) {
        if (seat.id() == seat_id) {
            return &seat;
        }
    }
    return nullptr;
}

const Seat* ReservationTable::find_seat_internal(std::string_view seat_id) const noexcept {
    for (const auto& seat : seats_) {
        if (seat.id() == seat_id) {
            return &seat;
        }
    }
    return nullptr;
}

} // namespace css223::core
