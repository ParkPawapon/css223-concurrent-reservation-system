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

    // 7. Verify invalid seat IDs are rejected
    assert(!table.has_seat("A6"));
    assert(!table.has_seat("E1"));
    assert(!table.has_seat("A0"));
    assert(!table.has_seat("ABC"));
    assert(!table.has_seat(""));

    assert(!table.get_seat("A6").has_value());
    assert(!table.get_seat("E1").has_value());

    // 8. Verify ReservationTable reserve behavior
    css223::core::ReservationTable reserve_table;

    // Valid reservation should succeed
    assert(reserve_table.reserve_seat("A1", 101));

    auto reserved_seat = reserve_table.get_seat("A1");
    assert(reserved_seat.has_value());
    assert(reserved_seat->is_reserved());
    assert(reserved_seat->owner_client_id().has_value());
    assert(reserved_seat->owner_client_id().value() == 101);

    // Another client cannot reserve the same seat
    assert(!reserve_table.reserve_seat("A1", 102));

    // Invalid seat ID cannot be reserved
    assert(!reserve_table.reserve_seat("A6", 101));

    // Invalid client ID (0) cannot reserve a seat
    assert(!reserve_table.reserve_seat(
        "A2", css223::common::kInvalidClientId
    ));

    // 9. Verify ReservationTable cancel behavior

    // Another client cannot cancel the reservation
    assert(!reserve_table.cancel_seat("A1", 102));

    auto still_reserved = reserve_table.get_seat("A1");
    assert(still_reserved.has_value());
    assert(still_reserved->is_reserved());
    assert(still_reserved->owner_client_id().value() == 101);

    // The owner can cancel the reservation
    assert(reserve_table.cancel_seat("A1", 101));

    auto cancelled_seat = reserve_table.get_seat("A1");
    assert(cancelled_seat.has_value());
    assert(cancelled_seat->is_available());
    assert(!cancelled_seat->owner_client_id().has_value());

    // Cannot cancel an already available seat
    assert(!reserve_table.cancel_seat("A1", 101));

    // Invalid seat ID cannot be cancelled
    assert(!reserve_table.cancel_seat("A6", 101));

    // 10. Verify delay action is called during reservation
    css223::core::ReservationTable delay_table;

    bool delay_called = false;

    auto delay_action = [&delay_called]() {
        delay_called = true;
    };

    assert(delay_table.reserve_seat("B1", 201, delay_action));
    assert(delay_called);

    auto delayed_seat = delay_table.get_seat("B1");
    assert(delayed_seat.has_value());
    assert(delayed_seat->is_reserved());
    assert(delayed_seat->owner_client_id().value() == 201);
    // Verify delay is not called when reservation fails
    bool failed_delay_called = false;

    auto failed_delay_action = [&failed_delay_called]() {
        failed_delay_called = true;
    };

    // B1 is already reserved by client 201
    assert(!delay_table.reserve_seat("B1", 202, failed_delay_action));

    // Delay should not be called because the seat was not available
    assert(!failed_delay_called);

    // Verify LIST behavior through get_all_seats()
    const auto all_seats = delay_table.get_all_seats();

    assert(all_seats.size() == 20);
    assert(all_seats.front().id() == "A1");
    assert(all_seats.back().id() == "D5");

    // Verify STATUS behavior through get_seat()
    const auto status_b1 = delay_table.get_seat("B1");

    assert(status_b1.has_value());
    assert(status_b1->is_reserved());
    assert(status_b1->owner_client_id().value() == 201);

    // Unknown seat should not have a status
    assert(!delay_table.get_seat("E1").has_value());

    // Verify reset_all returns every seat to AVAILABLE
    delay_table.reset_all();

    for (const auto& reset_seat : delay_table.get_all_seats()) {
        assert(reset_seat.is_available());
        assert(!reset_seat.owner_client_id().has_value());
    }

    // 11. Verify command parsing helpers
    assert(css223::common::parse_command_type("LIST") == css223::common::CommandType::List);
    assert(css223::common::parse_command_type("STATUS") == css223::common::CommandType::Status);
    assert(css223::common::parse_command_type("RESERVE") == css223::common::CommandType::Reserve);
    assert(css223::common::parse_command_type("CANCEL") == css223::common::CommandType::Cancel);
    assert(css223::common::parse_command_type("QUIT") == css223::common::CommandType::Quit);

    return EXIT_SUCCESS;
    
}
