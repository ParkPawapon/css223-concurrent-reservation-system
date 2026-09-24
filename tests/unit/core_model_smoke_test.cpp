#include <cassert>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "common/command.hpp"
#include "common/constants.hpp"
#include "common/result.hpp"
#include "core/reservation_table.hpp"
#include "core/seat.hpp"

int main() {
    // 1. Verify Seat initial state and invariants
    css223::core::Seat seat("A1");
    assert(seat.id() == "A1");
    assert(seat.is_available());
    assert(!seat.is_reserved());
    assert(!seat.owner_client_id().has_value());

    // 2. Verify Reserve transition
    assert(seat.reserve(101));
    assert(seat.is_reserved());
    assert(!seat.is_available());
    assert(seat.owner_client_id().value() == 101);

    // 3. Verify Cannot reserve an already reserved seat
    assert(!seat.reserve(102));
    assert(seat.owner_client_id().value() == 101);

    // 4. Verify Cancel by wrong owner fails
    assert(!seat.cancel(999));
    assert(seat.is_reserved());

    // 5. Verify Cancel by correct owner succeeds
    assert(seat.cancel(101));
    assert(seat.is_available());
    assert(!seat.owner_client_id().has_value());

    // 6. Verify ReservationTable canonical 20 seats
    css223::core::ReservationTable table;
    assert(table.seat_count() == css223::common::kTotalSeats);
    assert(table.seat_count() == 20);

    for (const auto& seat_id : css223::common::kCanonicalSeatIds) {
        assert(table.has_seat(seat_id));
        const auto seat_opt = table.get_seat(seat_id);
        assert(seat_opt.has_value());
        assert(seat_opt->is_available());
    }

    // 7. Verify command parsing helpers
    assert(css223::common::parse_command_type("LIST") == css223::common::CommandType::List);
    assert(css223::common::parse_command_type("STATUS") == css223::common::CommandType::Status);
    assert(css223::common::parse_command_type("RESERVE") == css223::common::CommandType::Reserve);
    assert(css223::common::parse_command_type("CANCEL") == css223::common::CommandType::Cancel);
    assert(css223::common::parse_command_type("QUIT") == css223::common::CommandType::Quit);

    return EXIT_SUCCESS;
}
