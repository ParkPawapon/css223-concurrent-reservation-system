#include <cctype>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "client/client.hpp"
#include "client/client_repl.hpp"
#include "client/command_parser.hpp"
#include "client/seat_map_formatter.hpp"
#include "client/terminal_ui.hpp"
#include "common/command.hpp"
#include "common/constants.hpp"
#include "common/types.hpp"
#include "core/reservation_table.hpp"
#include "core/seat.hpp"

namespace {

css223::client::Client* g_active_client = nullptr;

void signal_handler(int signal) {
    if (g_active_client != nullptr) {
        std::cerr << "\n[Client] Signal " << signal << " received. Cleaning up reply queue...\n";
        g_active_client->disconnect();
        g_active_client = nullptr;
    }
    std::_Exit(EXIT_FAILURE);
}

void print_usage(std::string_view program_name) {
    std::cout << "Usage: " << program_name << " [<client_id> | --preview]\n"
              << "Options:\n"
              << "  <client_id>  Positive integer client ID to connect to server via POSIX MQ\n"
              << "  --preview    Interactive Cinema Box Office terminal UI (no server needed)\n"
              << "Example:\n"
              << "  " << program_name << "\n"
              << "  " << program_name << " 1\n"
              << "  " << program_name << " --preview\n";
}

int run_preview_mode() {
    css223::client::TerminalUi::init_signal_handlers();

    css223::core::ReservationTable demo_table;
    demo_table.reserve_seat("A1", 1);
    demo_table.reserve_seat("B3", 2);
    demo_table.reserve_seat("C2", 1);

    auto get_display_seats = [&demo_table]() {
        std::vector<css223::client::SeatDisplayInfo> display_seats;
        for (const auto& seat : demo_table.get_all_seats()) {
            css223::client::SeatDisplayInfo info{};
            info.seat_id = std::string(seat.id());
            info.status = seat.status();
            info.owner_client_id =
                seat.owner_client_id().value_or(css223::common::kInvalidClientId);
            display_seats.push_back(std::move(info));
        }
        return display_seats;
    };

    std::string feedback_msg;
    std::string feedback_type;
    std::string ticket_seat_id;

    auto redraw = [&]() {
        if (css223::client::TerminalUi::is_interactive()) {
            css223::client::TerminalUi::render_kiosk_view(
                std::cout, get_display_seats(), 1, feedback_msg, feedback_type, ticket_seat_id);
        } else {
            std::cout << css223::client::TerminalUi::format_grid(get_display_seats(), false, 1)
                      << "\n";
            std::cout << css223::client::TerminalUi::format_prompt(1, false);
            std::cout.flush();
        }
    };

    redraw();

    std::string line;
    while (true) {
        if (css223::client::TerminalUi::has_resized()) {
            css223::client::TerminalUi::reset_resized();
            redraw();
        }

        if (!std::getline(std::cin, line)) {
            if (css223::client::TerminalUi::has_resized()) {
                std::cin.clear();
                css223::client::TerminalUi::reset_resized();
                redraw();
                continue;
            }
            std::cout << "\n"
                      << css223::client::TerminalUi::format_info(
                             "Leaving Cinema Theater. See you next show!")
                      << "\n";
            break;
        }

        line = css223::client::TerminalUi::sanitize_input(line);
        if (line.empty()) {
            redraw();
            continue;
        }

        auto parsed = css223::client::CommandParser::parse_line(line);
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

            // Interactive prompts for number shortcuts
            if (trimmed == "2" || trimmed == "RESERVE" || trimmed == "reserve") {
                std::cout << css223::client::TerminalUi::center_line(
                    "Enter Seat ID to reserve (e.g. A1 - D5): ");
                std::cout.flush();
                std::string seat;
                if (std::getline(std::cin, seat)) {
                    seat = css223::client::TerminalUi::sanitize_input(seat);
                    for (auto& c : seat) {
                        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    }
                    if (demo_table.reserve_seat(seat, 1)) {
                        ticket_seat_id = seat;
                        feedback_msg = "Seat " + seat + " reserved successfully for Client 1.";
                        feedback_type = "SUCCESS";
                    } else {
                        ticket_seat_id = "";
                        feedback_msg =
                            "Reservation failed: Seat " + seat + " is already reserved or invalid.";
                        feedback_type = "FAILED";
                    }
                    redraw();
                    continue;
                }
            } else if (trimmed == "3" || trimmed == "STATUS" || trimmed == "status") {
                std::cout << css223::client::TerminalUi::center_line(
                    "Enter Seat ID to check (e.g. A1 - D5): ");
                std::cout.flush();
                std::string seat;
                if (std::getline(std::cin, seat)) {
                    seat = css223::client::TerminalUi::sanitize_input(seat);
                    for (auto& c : seat) {
                        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    }
                    auto seat_info = demo_table.get_seat(seat);
                    if (seat_info.has_value()) {
                        css223::client::SeatDisplayInfo info{};
                        info.seat_id = std::string(seat_info->id());
                        info.status = seat_info->status();
                        info.owner_client_id =
                            seat_info->owner_client_id().value_or(css223::common::kInvalidClientId);
                        ticket_seat_id = "";
                        feedback_msg = css223::client::SeatMapFormatter::format_single_seat(info);
                        feedback_type = "INFO";
                    } else {
                        ticket_seat_id = "";
                        feedback_msg = "Invalid seat ID: " + seat;
                        feedback_type = "FAILED";
                    }
                    redraw();
                    continue;
                }
            } else if (trimmed == "4" || trimmed == "CANCEL" || trimmed == "cancel") {
                std::cout << css223::client::TerminalUi::center_line(
                    "Enter Seat ID to cancel (e.g. A1 - D5): ");
                std::cout.flush();
                std::string seat;
                if (std::getline(std::cin, seat)) {
                    seat = css223::client::TerminalUi::sanitize_input(seat);
                    for (auto& c : seat) {
                        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    }
                    ticket_seat_id = "";
                    if (demo_table.cancel_seat(seat, 1)) {
                        feedback_msg = "Reservation for Seat " + seat + " cancelled.";
                        feedback_type = "SUCCESS";
                    } else {
                        feedback_msg =
                            "Cancellation failed: Seat " + seat + " was not reserved by Client 1.";
                        feedback_type = "FAILED";
                    }
                    redraw();
                    continue;
                }
            }

            ticket_seat_id = "";
            feedback_msg = "Unknown command syntax. Type 'HELP' for instructions.";
            feedback_type = "FAILED";
            redraw();
            continue;
        }

        if (parsed->is_help) {
            ticket_seat_id = "";
            feedback_msg = "Select: 1 (LIST), 2 <seat> (RESERVE), 3 <seat> (STATUS), 4 <seat> "
                           "(CANCEL), 6 (QUIT)";
            feedback_type = "INFO";
            redraw();
            continue;
        }

        if (parsed->is_clear) {
            ticket_seat_id = "";
            feedback_msg = "";
            feedback_type = "";
            redraw();
            continue;
        }

        switch (parsed->type) {
            case css223::common::CommandType::List:
                ticket_seat_id = "";
                feedback_msg = "Seat table listed successfully";
                feedback_type = "INFO";
                redraw();
                break;
            case css223::common::CommandType::Status: {
                ticket_seat_id = "";
                auto seat = demo_table.get_seat(parsed->seat_id);
                if (seat.has_value()) {
                    css223::client::SeatDisplayInfo info{};
                    info.seat_id = std::string(seat->id());
                    info.status = seat->status();
                    info.owner_client_id =
                        seat->owner_client_id().value_or(css223::common::kInvalidClientId);
                    feedback_msg = css223::client::SeatMapFormatter::format_single_seat(info);
                    feedback_type = "INFO";
                } else {
                    feedback_msg = "Invalid seat ID: " + std::string(parsed->seat_id);
                    feedback_type = "FAILED";
                }
                redraw();
                break;
            }
            case css223::common::CommandType::Reserve: {
                if (demo_table.reserve_seat(parsed->seat_id, 1)) {
                    ticket_seat_id = std::string(parsed->seat_id);
                    feedback_msg = "Seat " + std::string(parsed->seat_id) +
                                   " reserved successfully for Client 1.";
                    feedback_type = "SUCCESS";
                } else {
                    ticket_seat_id = "";
                    feedback_msg = "Reservation failed: Seat " + std::string(parsed->seat_id) +
                                   " is already reserved or invalid.";
                    feedback_type = "FAILED";
                }
                redraw();
                break;
            }
            case css223::common::CommandType::Cancel: {
                ticket_seat_id = "";
                if (demo_table.cancel_seat(parsed->seat_id, 1)) {
                    feedback_msg =
                        "Reservation for Seat " + std::string(parsed->seat_id) + " cancelled.";
                    feedback_type = "SUCCESS";
                } else {
                    feedback_msg = "Cancellation failed: Seat " + std::string(parsed->seat_id) +
                                   " was not reserved by Client 1.";
                    feedback_type = "FAILED";
                }
                redraw();
                break;
            }
            case css223::common::CommandType::Quit:
                std::cout << "\n"
                          << css223::client::TerminalUi::format_info(
                                 "Leaving Cinema Theater. See you next show!")
                          << "\n";
                return EXIT_SUCCESS;
            case css223::common::CommandType::Unknown:
            default:
                ticket_seat_id = "";
                feedback_msg = "Unknown command. Type 'HELP' for instructions.";
                feedback_type = "FAILED";
                redraw();
                break;
        }
    }

    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        // Default to interactive Cinema UI preview if no arguments given
        return run_preview_mode();
    }

    std::string_view first_arg(argv[1]);
    if (first_arg == "--preview" || first_arg == "--demo" || first_arg == "-p") {
        return run_preview_mode();
    }

    if (first_arg == "--help" || first_arg == "-h") {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    int parsed_id = std::atoi(argv[1]);
    if (parsed_id <= 0) {
        std::cerr << "[Client] Error: client_id must be a positive integer or --preview.\n";
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    auto client_id = static_cast<css223::common::ClientId>(parsed_id);

    css223::client::Client client(client_id);
    g_active_client = &client;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    if (!client.connect()) {
        std::cerr << "[Client] Error: Unable to connect to server queue ("
                  << css223::common::kDefaultServerQueueName
                  << "). Please ensure the reservation server is running.\n";
        g_active_client = nullptr;
        return EXIT_FAILURE;
    }

    css223::client::ClientRepl repl(client);
    repl.run(std::cin, std::cout);

    g_active_client = nullptr;
    return EXIT_SUCCESS;
}
