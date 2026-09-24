#ifndef RESERVATION_CORE_SEAT_HPP
#define RESERVATION_CORE_SEAT_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "common/types.hpp"

namespace css223::core {

enum class SeatStatus : std::uint8_t { Available = 0, Reserved = 1 };

[[nodiscard]] constexpr std::string_view to_string(SeatStatus status) noexcept {
    switch (status) {
        case SeatStatus::Available:
            return "AVAILABLE";
        case SeatStatus::Reserved:
            return "RESERVED";
        default:
            return "UNKNOWN";
    }
}

class Seat {
public:
    explicit Seat(std::string seat_id);

    [[nodiscard]] const std::string& id() const noexcept { return id_; }
    [[nodiscard]] SeatStatus status() const noexcept { return status_; }
    [[nodiscard]] std::optional<common::ClientId> owner_client_id() const noexcept {
        return owner_client_id_;
    }

    [[nodiscard]] bool is_available() const noexcept { return status_ == SeatStatus::Available; }
    [[nodiscard]] bool is_reserved() const noexcept { return status_ == SeatStatus::Reserved; }

    bool reserve(common::ClientId client_id);
    bool cancel(common::ClientId client_id);
    void reset() noexcept;

private:
    std::string id_;
    SeatStatus status_{SeatStatus::Available};
    std::optional<common::ClientId> owner_client_id_{std::nullopt};
};

} // namespace css223::core

#endif // RESERVATION_CORE_SEAT_HPP
