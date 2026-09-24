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
    css223::client::TerminalUi::show_welcome_banner(std::cout, 1, true);

    css223::core::ReservationTable demo_table;
    demo_table.reserve_seat("A1", 1);
    demo_table.reserve_seat("B3", 2);
    demo_table.reserve_seat("C2", 1);

    auto render_grid = [&demo_table]() {
        std::vector<css223::client::SeatDisplayInfo> display_seats;
        for (const auto& seat : demo_table.get_all_seats()) {
            css223::client::SeatDisplayInfo info{};
            info.seat_id = std::string(seat.id());
            info.status = seat.status();
            info.owner_client_id =
                seat.owner_client_id().value_or(css223::common::kInvalidClientId);
            display_seats.push_back(std::move(info));
        }
        return css223::client::TerminalUi::format_grid(display_seats);
    };

    std::cout << render_grid() << "\n";

    std::string line;
    while (true) {
        std::cout << css223::client::TerminalUi::format_prompt(1);
        std::cout.flush();

        if (!std::getline(std::cin, line)) {
            std::cout << "\n"
                      << css223::client::TerminalUi::format_info(
                             "Leaving Cinema Theater. See you next show!")
                      << "\n";
            break;
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
            if (trimmed.empty()) {
                continue;
            }

            // Interactive prompts for number shortcuts
            if (trimmed == "2" || trimmed == "RESERVE" || trimmed == "reserve") {
                std::cout << css223::client::TerminalUi::center_line(
                    "Enter Seat ID to reserve (e.g. A1 - D5): ");
                std::cout.flush();
                std::string seat;
                if (std::getline(std::cin, seat)) {
                    for (auto& c : seat) {
                        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    }
                    if (demo_table.reserve_seat(seat, 1)) {
                        css223::client::TerminalUi::animate_ticket_print(std::cout, seat, 1);
                        css223::client::TerminalUi::print_ticket_stub(std::cout, seat, 1);
                        std::string msg = "Seat " + seat + " reserved successfully for Client 1.";
                        std::cout << css223::client::TerminalUi::format_success(msg) << "\n";
                        std::cout << render_grid() << "\n";
                    } else {
                        std::string msg =
                            "Reservation failed: Seat " + seat + " is already reserved or invalid.";
                        std::cout << css223::client::TerminalUi::format_error(msg) << "\n";
                    }
                    continue;
                }
            } else if (trimmed == "3" || trimmed == "STATUS" || trimmed == "status") {
                std::cout << css223::client::TerminalUi::center_line(
                    "Enter Seat ID to check (e.g. A1 - D5): ");
                std::cout.flush();
                std::string seat;
                if (std::getline(std::cin, seat)) {
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
                        std::cout << css223::client::TerminalUi::format_info(
                                         css223::client::SeatMapFormatter::format_single_seat(info))
                                  << "\n";
                    } else {
                        std::cout << css223::client::TerminalUi::format_error("Invalid seat ID: " +
                                                                              seat)
                                  << "\n";
                    }
                    continue;
                }
            } else if (trimmed == "4" || trimmed == "CANCEL" || trimmed == "cancel") {
                std::cout << css223::client::TerminalUi::center_line(
                    "Enter Seat ID to cancel (e.g. A1 - D5): ");
                std::cout.flush();
                std::string seat;
                if (std::getline(std::cin, seat)) {
                    for (auto& c : seat) {
                        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    }
                    if (demo_table.cancel_seat(seat, 1)) {
                        std::string msg = "Reservation for Seat " + seat + " cancelled.";
                        std::cout << css223::client::TerminalUi::format_success(msg) << "\n";
                        std::cout << render_grid() << "\n";
                    } else {
                        std::string msg =
                            "Cancellation failed: Seat " + seat + " was not reserved by Client 1.";
                        std::cout << css223::client::TerminalUi::format_error(msg) << "\n";
                    }
                    continue;
                }
            }

            std::cout << css223::client::TerminalUi::format_error(
                             "Unknown command syntax. Type 'HELP' for instructions.")
                      << "\n";
            continue;
        }

        if (parsed->is_help) {
            css223::client::TerminalUi::show_help_box(std::cout);
            continue;
        }

        if (parsed->is_clear) {
            css223::client::TerminalUi::clear_screen(std::cout);
            std::cout << render_grid() << "\n";
            continue;
        }

        switch (parsed->type) {
            case css223::common::CommandType::List:
                std::cout << render_grid() << "\n";
                break;
            case css223::common::CommandType::Status: {
                auto seat = demo_table.get_seat(parsed->seat_id);
                if (seat.has_value()) {
                    css223::client::SeatDisplayInfo info{};
                    info.seat_id = std::string(seat->id());
                    info.status = seat->status();
                    info.owner_client_id =
                        seat->owner_client_id().value_or(css223::common::kInvalidClientId);
                    std::cout << css223::client::TerminalUi::format_info(
                                     css223::client::SeatMapFormatter::format_single_seat(info))
                              << "\n";
                } else {
                    std::cout << css223::client::TerminalUi::format_error(
                                     "Invalid seat ID: " + std::string(parsed->seat_id))
                              << "\n";
                }
                break;
            }
            case css223::common::CommandType::Reserve: {
                if (demo_table.reserve_seat(parsed->seat_id, 1)) {
                    css223::client::TerminalUi::animate_ticket_print(std::cout, parsed->seat_id, 1);
                    css223::client::TerminalUi::print_ticket_stub(std::cout, parsed->seat_id, 1);
                    std::string msg = "Seat " + std::string(parsed->seat_id) +
                                      " reserved successfully for Client 1.";
                    std::cout << css223::client::TerminalUi::format_success(msg) << "\n";
                    std::cout << render_grid() << "\n";
                } else {
                    std::string msg = "Reservation failed: Seat " + std::string(parsed->seat_id) +
                                      " is already reserved or invalid.";
                    std::cout << css223::client::TerminalUi::format_error(msg) << "\n";
                }
                break;
            }
            case css223::common::CommandType::Cancel: {
                if (demo_table.cancel_seat(parsed->seat_id, 1)) {
                    std::string msg =
                        "Reservation for Seat " + std::string(parsed->seat_id) + " cancelled.";
                    std::cout << css223::client::TerminalUi::format_success(msg) << "\n";
                    std::cout << render_grid() << "\n";
                } else {
                    std::string msg = "Cancellation failed: Seat " + std::string(parsed->seat_id) +
                                      " was not reserved by Client 1.";
                    std::cout << css223::client::TerminalUi::format_error(msg) << "\n";
                }
                break;
            }
            case css223::common::CommandType::Quit:
                std::cout << css223::client::TerminalUi::format_info(
                                 "Leaving Cinema Theater. See you next show!")
                          << "\n";
                return EXIT_SUCCESS;
            case css223::common::CommandType::Unknown:
            default:
                std::cout << css223::client::TerminalUi::format_error(
                                 "Unknown command. Type 'HELP' for instructions.")
                          << "\n";
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
