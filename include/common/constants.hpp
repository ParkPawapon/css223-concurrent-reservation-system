#ifndef RESERVATION_COMMON_CONSTANTS_HPP
#define RESERVATION_COMMON_CONSTANTS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace css223::common {

inline constexpr std::size_t kTotalSeats = 20;
inline constexpr std::size_t kMinClients = 5;
inline constexpr std::size_t kDefaultWorkerCount = 3;
inline constexpr std::size_t kMinWorkerCount = 1;

inline constexpr unsigned int kDefaultMinDelayMs = 50;
inline constexpr unsigned int kDefaultMaxDelayMs = 500;

inline constexpr std::size_t kMaxSeatIdLength = 8;
inline constexpr std::size_t kMaxQueueNameLength = 64;
inline constexpr std::size_t kMaxMessageTextLength = 128;

inline constexpr std::string_view kDefaultServerQueueName = "/css223_reservation_requests";
inline constexpr std::string_view kClientQueuePrefix = "/css223_client_";

inline constexpr std::array<std::string_view, kTotalSeats> kCanonicalSeatIds = {
    "A1", "A2", "A3", "A4", "A5", "B1", "B2", "B3", "B4", "B5",
    "C1", "C2", "C3", "C4", "C5", "D1", "D2", "D3", "D4", "D5"};

} // namespace css223::common

#endif // RESERVATION_COMMON_CONSTANTS_HPP
