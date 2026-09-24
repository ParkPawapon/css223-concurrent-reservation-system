#include "client/command_parser.hpp"

#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include "common/command.hpp"

namespace css223::client {

std::optional<ParsedCommand> CommandParser::parse_line(std::string_view input_line) {
    if (input_line.empty()) {
        return std::nullopt;
    }

    std::istringstream stream{std::string(input_line)};
    std::string verb;
    if (!(stream >> verb)) {
        return std::nullopt;
    }

    for (auto& c : verb) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    common::CommandType type = common::parse_command_type(verb);
    if (type == common::CommandType::Unknown) {
        return std::nullopt;
    }

    ParsedCommand cmd{};
    cmd.type = type;

    if (type == common::CommandType::Status || type == common::CommandType::Reserve ||
        type == common::CommandType::Cancel) {
        std::string seat;
        if (stream >> seat) {
            for (auto& c : seat) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            cmd.seat_id = seat;
        } else {
            return std::nullopt;
        }
    }

    return cmd;
}

} // namespace css223::client
