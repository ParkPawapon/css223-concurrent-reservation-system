#ifndef RESERVATION_COMMON_RESULT_HPP
#define RESERVATION_COMMON_RESULT_HPP

#include <cstdint>
#include <string_view>

namespace css223::common {

enum class StatusCode : std::uint8_t { Success = 0, Failure = 1 };

[[nodiscard]] constexpr std::string_view to_string(StatusCode status) noexcept {
    switch (status) {
        case StatusCode::Success:
            return "SUCCESS";
        case StatusCode::Failure:
        default:
            return "FAILURE";
    }
}

} // namespace css223::common

#endif // RESERVATION_COMMON_RESULT_HPP
