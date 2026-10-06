#include "client/command_parser.hpp"

#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include "client/terminal_ui.hpp"
#include "common/command.hpp"

namespace css223::client {

std::optional<ParsedCommand> CommandParser::parse_line(std::string_view input_line) {
    std::string sanitized = TerminalUi::sanitize_input(input_line);
    if (sanitized.empty()) {
        return std::nullopt;
    }

    std::istringstream stream{sanitized};
    std::string verb;
    if (!(stream >> verb)) {
        return std::nullopt;
    }

    for (auto& c : verb) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    if (verb == "HELP" || verb == "?" || verb == "H" || verb == "5") {
        ParsedCommand help_cmd{};
        help_cmd.is_help = true;
        return help_cmd;
    }

    if (verb == "CLEAR" || verb == "CLS" || verb == "REFRESH" || verb == "7") {
        ParsedCommand clear_cmd{};
        clear_cmd.is_clear = true;
        return clear_cmd;
    }

    if (verb == "EXIT" || verb == "QUIT" || verb == "Q" || verb == "6") {
        ParsedCommand exit_cmd{};
        exit_cmd.type = common::CommandType::Quit;
        return exit_cmd;
    }

    if (verb == "1" || verb == "L") {
        verb = "LIST";
    } else if (verb == "2" || verb == "R") {
        verb = "RESERVE";
    } else if (verb == "3" || verb == "S") {
        verb = "STATUS";
    } else if (verb == "4" || verb == "C") {
        verb = "CANCEL";
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
