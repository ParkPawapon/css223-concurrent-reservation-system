#ifndef RESERVATION_COMMON_COMMAND_HPP
#define RESERVATION_COMMON_COMMAND_HPP

#include <cstdint>
#include <string_view>

namespace css223::common {

enum class CommandType : std::uint8_t {
    List = 1,
    Status = 2,
    Reserve = 3,
    Cancel = 4,
    Quit = 5,
    Unknown = 0
};

[[nodiscard]] constexpr std::string_view to_string(CommandType command) noexcept {
    switch (command) {
        case CommandType::List:
            return "LIST";
        case CommandType::Status:
            return "STATUS";
        case CommandType::Reserve:
            return "RESERVE";
        case CommandType::Cancel:
            return "CANCEL";
        case CommandType::Quit:
            return "QUIT";
        case CommandType::Unknown:
        default:
            return "UNKNOWN";
    }
}

[[nodiscard]] constexpr CommandType parse_command_type(std::string_view command_name) noexcept {
    if (command_name == "LIST") {
        return CommandType::List;
    }
    if (command_name == "STATUS") {
        return CommandType::Status;
    }
    if (command_name == "RESERVE") {
        return CommandType::Reserve;
    }
    if (command_name == "CANCEL") {
        return CommandType::Cancel;
    }
    if (command_name == "QUIT") {
        return CommandType::Quit;
    }
    return CommandType::Unknown;
}

} // namespace css223::common

#endif // RESERVATION_COMMON_COMMAND_HPP
