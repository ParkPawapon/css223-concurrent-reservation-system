#include "client/client_repl.hpp"

#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "client/command_parser.hpp"
#include "client/seat_map_formatter.hpp"
#include "client/terminal_ui.hpp"
#include "common/command.hpp"
#include "common/result.hpp"
#include "ipc/message.hpp"

namespace css223::client {

ClientRepl::ClientRepl(Client& client) noexcept : client_(client) {}

void ClientRepl::handle_help(std::ostream& out) {
    TerminalUi::show_help_box(out);
}

void ClientRepl::handle_list(std::ostream& out) {
    auto seat_map = client_.fetch_seat_map();
    if (seat_map.empty()) {
        out << TerminalUi::format_error("Unable to retrieve seat map from server.") << "\n";
    } else {
        out << TerminalUi::format_grid(seat_map) << "\n";
    }
}

void ClientRepl::handle_status(std::string_view seat_id, std::ostream& out) {
    auto response = client_.request_status(seat_id);
    if (!response.has_value()) {
        std::string err_msg = "Failed to receive response for STATUS " + std::string(seat_id);
        out << TerminalUi::format_error(err_msg) << "\n";
        return;
    }

    if (response->result == common::StatusCode::Success) {
        SeatDisplayInfo info{};
        info.seat_id = std::string(seat_id);
        info.status = response->status;
        info.owner_client_id = response->owner_client_id;
        std::string info_msg = SeatMapFormatter::format_single_seat(info);
        out << TerminalUi::format_info(info_msg) << "\n";
    } else {
        std::string_view msg =
            ipc::buffer_to_string_view(response->message, sizeof(response->message));
        std::string err_msg = "Seat " + std::string(seat_id) + " check failed: " + std::string(msg);
        out << TerminalUi::format_error(err_msg) << "\n";
    }
}

void ClientRepl::handle_reserve(std::string_view seat_id, std::ostream& out) {
    auto response = client_.request_reserve(seat_id);
    if (!response.has_value()) {
        std::string err_msg = "Failed to receive response for RESERVE " + std::string(seat_id);
        out << TerminalUi::format_error(err_msg) << "\n";
        return;
    }

    if (response->result == common::StatusCode::Success) {
        TerminalUi::animate_ticket_print(out, seat_id, client_.client_id());
        TerminalUi::print_ticket_stub(out, seat_id, client_.client_id());
        std::ostringstream ss;
        ss << "Seat " << seat_id << " reserved successfully for Client " << client_.client_id()
           << ".";
        out << TerminalUi::format_success(ss.str()) << "\n";
    } else {
        std::string_view msg =
            ipc::buffer_to_string_view(response->message, sizeof(response->message));
        std::string err_msg =
            "Reservation failed for Seat " + std::string(seat_id) + ": " + std::string(msg);
        out << TerminalUi::format_error(err_msg) << "\n";
    }
}

void ClientRepl::handle_cancel(std::string_view seat_id, std::ostream& out) {
    auto response = client_.request_cancel(seat_id);
    if (!response.has_value()) {
        std::string err_msg = "Failed to receive response for CANCEL " + std::string(seat_id);
        out << TerminalUi::format_error(err_msg) << "\n";
        return;
    }

    if (response->result == common::StatusCode::Success) {
        std::string ok_msg = "Reservation for Seat " + std::string(seat_id) + " cancelled.";
        out << TerminalUi::format_success(ok_msg) << "\n";
    } else {
        std::string_view msg =
            ipc::buffer_to_string_view(response->message, sizeof(response->message));
        std::string err_msg =
            "Cancellation failed for Seat " + std::string(seat_id) + ": " + std::string(msg);
        out << TerminalUi::format_error(err_msg) << "\n";
    }
}

void ClientRepl::handle_quit(std::ostream& out) {
    static_cast<void>(client_.request_quit());
    client_.disconnect();
    out << TerminalUi::format_info(
               "Leaving Cinema Theater. Reply queue unlinked. See you next show!")
        << "\n";
}

void ClientRepl::run(std::istream& in, std::ostream& out) {
    TerminalUi::show_welcome_banner(out, client_.client_id(), true);

    std::string line;
    while (true) {
        out << TerminalUi::format_prompt(client_.client_id());
        out.flush();

        if (!std::getline(in, line)) {
            handle_quit(out);
            break;
        }

        auto parsed = CommandParser::parse_line(line);
        if (!parsed.has_value()) {
            std::string_view trimmed(line);
            while (!trimmed.empty() &&
                   std::isspace(static_cast<unsigned char>(trimmed.front())) != 0) {
                trimmed.remove_prefix(1);
            }
            while (!trimmed.empty() &&
                   std::isspace(static_cast<unsigned char>(trimmed.back())) != 0) {
                trimmed.remove_suffix(1);
            }
            if (trimmed.empty()) {
                continue;
            }

            // Interactive prompts for number shortcuts
            if (trimmed == "2" || trimmed == "RESERVE" || trimmed == "reserve") {
                out << TerminalUi::center_line("Enter Seat ID to reserve (e.g. A1 - D5): ");
                out.flush();
                std::string seat;
                if (std::getline(in, seat)) {
                    for (auto& c : seat) {
                        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    }
                    handle_reserve(seat, out);
                    continue;
                }
            } else if (trimmed == "3" || trimmed == "STATUS" || trimmed == "status") {
                out << TerminalUi::center_line("Enter Seat ID to check (e.g. A1 - D5): ");
                out.flush();
                std::string seat;
                if (std::getline(in, seat)) {
                    for (auto& c : seat) {
                        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    }
                    handle_status(seat, out);
                    continue;
                }
            } else if (trimmed == "4" || trimmed == "CANCEL" || trimmed == "cancel") {
                out << TerminalUi::center_line("Enter Seat ID to cancel (e.g. A1 - D5): ");
                out.flush();
                std::string seat;
                if (std::getline(in, seat)) {
                    for (auto& c : seat) {
                        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    }
                    handle_cancel(seat, out);
                    continue;
                }
            }

            out << TerminalUi::format_error("Unknown command syntax. Type 'HELP' for instructions.")
                << "\n";
            continue;
        }

        if (parsed->is_help) {
            handle_help(out);
            continue;
        }

        if (parsed->is_clear) {
            TerminalUi::clear_screen(out);
            handle_list(out);
            continue;
        }

        switch (parsed->type) {
            case common::CommandType::List:
                handle_list(out);
                break;
            case common::CommandType::Status:
                handle_status(parsed->seat_id, out);
                break;
            case common::CommandType::Reserve:
                handle_reserve(parsed->seat_id, out);
                break;
            case common::CommandType::Cancel:
                handle_cancel(parsed->seat_id, out);
                break;
            case common::CommandType::Quit:
                handle_quit(out);
                return;
            case common::CommandType::Unknown:
            default:
                out << TerminalUi::format_error("Unknown command. Type 'HELP' for instructions.")
                    << "\n";
                break;
        }
    }
}

} // namespace css223::client
