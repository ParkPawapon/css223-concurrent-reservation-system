#ifndef RESERVATION_CORE_RESERVATION_TABLE_HPP
#define RESERVATION_CORE_RESERVATION_TABLE_HPP

#include <cstddef>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

#include "common/types.hpp"
#include "core/seat.hpp"

namespace css223::core {

class ReservationTable {
public:
    ReservationTable();

    [[nodiscard]] std::size_t seat_count() const noexcept;
    [[nodiscard]] bool has_seat(std::string_view seat_id) const noexcept;
    [[nodiscard]] std::optional<Seat> get_seat(std::string_view seat_id) const;
    [[nodiscard]] std::vector<Seat> get_all_seats() const;

    using DelayAction = std::function<void()>;

    bool reserve_seat(std::string_view seat_id,
                      common::ClientId client_id,
                      const DelayAction& delay_action = nullptr);

    bool cancel_seat(std::string_view seat_id,
                     common::ClientId client_id,
                     const DelayAction& delay_action = nullptr);

    void reset_all() noexcept;

private:
    [[nodiscard]] Seat* find_seat_internal(std::string_view seat_id) noexcept;
    [[nodiscard]] const Seat* find_seat_internal(std::string_view seat_id) const noexcept;

    std::vector<Seat> seats_;
};

} // namespace css223::core

#endif // RESERVATION_CORE_RESERVATION_TABLE_HPP
