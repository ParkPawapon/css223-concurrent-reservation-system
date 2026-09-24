#include "client/seat_map_formatter.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "common/constants.hpp"
#include "common/types.hpp"
#include "core/seat.hpp"

namespace css223::client {

std::string SeatMapFormatter::format_seat_token(const SeatDisplayInfo& seat) {
    std::ostringstream token;
    token << "[" << seat.seat_id << ": ";
    if (seat.status == core::SeatStatus::Available) {
        token << "AVAILABLE   ]";
    } else {
        token << "RSV (C#" << seat.owner_client_id << ")]";
    }
    return token.str();
}

std::string SeatMapFormatter::format_single_seat(const SeatDisplayInfo& seat) {
    std::ostringstream oss;
    oss << "Seat: " << seat.seat_id << " | Status: " << core::to_string(seat.status);
    if (seat.status == core::SeatStatus::Reserved &&
        seat.owner_client_id != common::kInvalidClientId) {
        oss << " | Owner: Client " << seat.owner_client_id;
    }
    return oss.str();
}

std::string SeatMapFormatter::format_grid(const std::vector<SeatDisplayInfo>& seats) {
    std::ostringstream out;
    constexpr std::size_t kLineWidth = 82;
    const std::string kSeparator(kLineWidth, '=');
    const std::string kSubSeparator(kLineWidth, '-');

    out << "\n" << kSeparator << "\n";
    out << std::string(32, ' ') << "[ CINEMA SCREEN ]\n";
    out << kSeparator << "\n";

    std::size_t available_count = 0;
    std::size_t reserved_count = 0;

    char current_row = '\0';
    for (const auto& seat : seats) {
        if (seat.seat_id.empty()) {
            continue;
        }

        if (seat.status == core::SeatStatus::Available) {
            ++available_count;
        } else {
            ++reserved_count;
        }

        char row_char = seat.seat_id.front();
        if (row_char != current_row) {
            if (current_row != '\0') {
                out << "\n";
            }
            current_row = row_char;
            out << "  Row " << current_row << ": ";
        } else {
            out << " ";
        }

        out << std::left << std::setw(15) << format_seat_token(seat);
    }

    out << "\n" << kSubSeparator << "\n";
    out << "  Summary: Total Seats = " << (available_count + reserved_count)
        << " | Available = " << available_count << " | Reserved = " << reserved_count << "\n";
    out << kSeparator << "\n";

    return out.str();
}

} // namespace css223::client
