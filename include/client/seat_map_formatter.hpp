#ifndef RESERVATION_CLIENT_SEAT_MAP_FORMATTER_HPP
#define RESERVATION_CLIENT_SEAT_MAP_FORMATTER_HPP

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "common/constants.hpp"
#include "common/types.hpp"
#include "core/seat.hpp"

namespace css223::client {

struct SeatDisplayInfo {
    std::string seat_id;
    core::SeatStatus status{core::SeatStatus::Available};
    common::ClientId owner_client_id{common::kInvalidClientId};
};

class SeatMapFormatter {
public:
    [[nodiscard]] static std::string format_grid(const std::vector<SeatDisplayInfo>& seats);
    [[nodiscard]] static std::string format_single_seat(const SeatDisplayInfo& seat);
    [[nodiscard]] static std::string format_seat_token(const SeatDisplayInfo& seat);
};

} // namespace css223::client

#endif // RESERVATION_CLIENT_SEAT_MAP_FORMATTER_HPP
