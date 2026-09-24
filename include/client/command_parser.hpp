#ifndef RESERVATION_CLIENT_COMMAND_PARSER_HPP
#define RESERVATION_CLIENT_COMMAND_PARSER_HPP

#include <optional>
#include <string>
#include <string_view>

#include "common/command.hpp"

namespace css223::client {

struct ParsedCommand {
    common::CommandType type{common::CommandType::Unknown};
    std::string seat_id;
    bool is_help{false};
    bool is_clear{false};
};

class CommandParser {
public:
    [[nodiscard]] static std::optional<ParsedCommand> parse_line(std::string_view input_line);
};

} // namespace css223::client

#endif // RESERVATION_CLIENT_COMMAND_PARSER_HPP
