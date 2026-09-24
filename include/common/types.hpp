#ifndef RESERVATION_COMMON_TYPES_HPP
#define RESERVATION_COMMON_TYPES_HPP

#include <cstdint>

namespace css223::common {

using ClientId = std::uint32_t;
using RequestId = std::uint32_t;

inline constexpr ClientId kInvalidClientId = 0;
inline constexpr RequestId kInvalidRequestId = 0;

} // namespace css223::common

#endif // RESERVATION_COMMON_TYPES_HPP
