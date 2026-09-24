#include <cassert>
#include <cstdlib>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "client/client.hpp"
#include "client/client_repl.hpp"
#include "client/command_parser.hpp"
#include "client/seat_map_formatter.hpp"
#include "client/terminal_ui.hpp"
#include "common/command.hpp"
#include "common/constants.hpp"
#include "core/seat.hpp"

namespace {

void test_command_parser_valid_commands() {
    // 1. LIST parsing (with various cases and whitespaces)
    auto list_cmd = css223::client::CommandParser::parse_line("LIST");
    assert(list_cmd.has_value());
    assert(list_cmd->type == css223::common::CommandType::List);
    assert(!list_cmd->is_help);

    auto list_lower = css223::client::CommandParser::parse_line("  list  ");
    assert(list_lower.has_value());
    assert(list_lower->type == css223::common::CommandType::List);

    // 2. STATUS parsing
    auto status_cmd = css223::client::CommandParser::parse_line("STATUS a1");
    assert(status_cmd.has_value());
    assert(status_cmd->type == css223::common::CommandType::Status);
    assert(status_cmd->seat_id == "A1");

    // 3. RESERVE parsing
    auto reserve_cmd = css223::client::CommandParser::parse_line("reserve   b3 ");
    assert(reserve_cmd.has_value());
    assert(reserve_cmd->type == css223::common::CommandType::Reserve);
    assert(reserve_cmd->seat_id == "B3");

    // 4. CANCEL parsing
    auto cancel_cmd = css223::client::CommandParser::parse_line("CANCEL C4");
    assert(cancel_cmd.has_value());
    assert(cancel_cmd->type == css223::common::CommandType::Cancel);
    assert(cancel_cmd->seat_id == "C4");

    // 5. QUIT and EXIT parsing
    auto quit_cmd = css223::client::CommandParser::parse_line("QUIT");
    assert(quit_cmd.has_value());
    assert(quit_cmd->type == css223::common::CommandType::Quit);

    auto exit_cmd = css223::client::CommandParser::parse_line("exit");
    assert(exit_cmd.has_value());
    assert(exit_cmd->type == css223::common::CommandType::Quit);

    // 6. HELP, H, ?, and 5 parsing
    auto help_cmd = css223::client::CommandParser::parse_line("HELP");
    assert(help_cmd.has_value());
    assert(help_cmd->is_help);

    auto h_cmd = css223::client::CommandParser::parse_line("H");
    assert(h_cmd.has_value());
    assert(h_cmd->is_help);

    auto h_lower = css223::client::CommandParser::parse_line("h");
    assert(h_lower.has_value());
    assert(h_lower->is_help);

    auto question_cmd = css223::client::CommandParser::parse_line("?");
    assert(question_cmd.has_value());
    assert(question_cmd->is_help);

    auto num_help_cmd = css223::client::CommandParser::parse_line("5");
    assert(num_help_cmd.has_value());
    assert(num_help_cmd->is_help);

    // 7. Numeric shortcuts parsing
    auto num_list_cmd = css223::client::CommandParser::parse_line("1");
    assert(num_list_cmd.has_value());
    assert(num_list_cmd->type == css223::common::CommandType::List);

    auto num_reserve_cmd = css223::client::CommandParser::parse_line("2 a1");
    assert(num_reserve_cmd.has_value());
    assert(num_reserve_cmd->type == css223::common::CommandType::Reserve);
    assert(num_reserve_cmd->seat_id == "A1");

    auto num_status_cmd = css223::client::CommandParser::parse_line("3 b2");
    assert(num_status_cmd.has_value());
    assert(num_status_cmd->type == css223::common::CommandType::Status);
    assert(num_status_cmd->seat_id == "B2");

    auto num_cancel_cmd = css223::client::CommandParser::parse_line("4 c3");
    assert(num_cancel_cmd.has_value());
    assert(num_cancel_cmd->type == css223::common::CommandType::Cancel);
    assert(num_cancel_cmd->seat_id == "C3");

    auto num_quit_cmd = css223::client::CommandParser::parse_line("6");
    assert(num_quit_cmd.has_value());
    assert(num_quit_cmd->type == css223::common::CommandType::Quit);

    auto q_cmd = css223::client::CommandParser::parse_line("q");
    assert(q_cmd.has_value());
    assert(q_cmd->type == css223::common::CommandType::Quit);

    // 8. CLEAR and CLS parsing
    auto clear_cmd = css223::client::CommandParser::parse_line("CLEAR");
    assert(clear_cmd.has_value());
    assert(clear_cmd->is_clear);

    auto cls_cmd = css223::client::CommandParser::parse_line("cls");
    assert(cls_cmd.has_value());
    assert(cls_cmd->is_clear);

    auto num_clear_cmd = css223::client::CommandParser::parse_line("7");
    assert(num_clear_cmd.has_value());
    assert(num_clear_cmd->is_clear);
}

void test_command_parser_invalid_commands() {
    // Missing seat arguments
    assert(!css223::client::CommandParser::parse_line("STATUS").has_value());
    assert(!css223::client::CommandParser::parse_line("RESERVE").has_value());
    assert(!css223::client::CommandParser::parse_line("CANCEL").has_value());

    // Empty or whitespace only
    assert(!css223::client::CommandParser::parse_line("").has_value());
    assert(!css223::client::CommandParser::parse_line("   \t  ").has_value());

    // Unknown verbs
    assert(!css223::client::CommandParser::parse_line("FOOBAR").has_value());
    assert(!css223::client::CommandParser::parse_line("BUY A1").has_value());
}

void test_seat_map_formatter() {
    std::vector<css223::client::SeatDisplayInfo> seats;
    seats.reserve(css223::common::kCanonicalSeatIds.size());

    for (std::size_t i = 0; i < css223::common::kCanonicalSeatIds.size(); ++i) {
        css223::client::SeatDisplayInfo info{};
        info.seat_id = std::string(css223::common::kCanonicalSeatIds[i]);
        if (info.seat_id == "A1") {
            info.status = css223::core::SeatStatus::Reserved;
            info.owner_client_id = 1;
        } else if (info.seat_id == "B3") {
            info.status = css223::core::SeatStatus::Reserved;
            info.owner_client_id = 42;
        } else {
            info.status = css223::core::SeatStatus::Available;
            info.owner_client_id = css223::common::kInvalidClientId;
        }
        seats.push_back(std::move(info));
    }

    std::string grid = css223::client::SeatMapFormatter::format_grid(seats);
    assert(grid.find("CINEMA SCREEN") != std::string::npos);
    assert(grid.find("Row A:") != std::string::npos);
    assert(grid.find("Row B:") != std::string::npos);
    assert(grid.find("Row C:") != std::string::npos);
    assert(grid.find("Row D:") != std::string::npos);
    assert(grid.find("Total Seats = 20") != std::string::npos);
    assert(grid.find("Available = 18") != std::string::npos);
    assert(grid.find("Reserved = 2") != std::string::npos);

    // Test single seat formatting
    css223::client::SeatDisplayInfo avail_seat{"A2", css223::core::SeatStatus::Available, 0};
    std::string avail_str = css223::client::SeatMapFormatter::format_single_seat(avail_seat);
    assert(avail_str.find("A2") != std::string::npos);
    assert(avail_str.find("AVAILABLE") != std::string::npos);

    css223::client::SeatDisplayInfo rsv_seat{"A1", css223::core::SeatStatus::Reserved, 5};
    std::string rsv_str = css223::client::SeatMapFormatter::format_single_seat(rsv_seat);
    assert(rsv_str.find("A1") != std::string::npos);
    assert(rsv_str.find("RESERVED") != std::string::npos);
    assert(rsv_str.find("Client 5") != std::string::npos);
}

void test_terminal_ui_helpers() {
    std::string prompt = css223::client::TerminalUi::format_prompt(5, false);
    assert(prompt == "Client[5]> ");

    std::string succ = css223::client::TerminalUi::format_success("Done", false);
    assert(succ.find("[SUCCESS] Done") != std::string::npos);

    std::string err = css223::client::TerminalUi::format_error("Failed", false);
    assert(err.find("[FAILED] Failed") != std::string::npos);

    std::string inf = css223::client::TerminalUi::format_info("Notice", false);
    assert(inf.find("[INFO] Notice") != std::string::npos);

    std::ostringstream banner_out;
    css223::client::TerminalUi::show_welcome_banner(banner_out, 7, false);
    std::string banner_str = banner_out.str();
    assert(banner_str.find("WELCOME TO CSS223 CINEMA THEATER") != std::string::npos);
    assert(banner_str.find("Connected to Box Office as Client ID: 7") != std::string::npos);
    assert(banner_str.find("Box Office Counter") != std::string::npos);
    assert(banner_str.find("LIST") != std::string::npos);

    std::ostringstream stub_out;
    css223::client::TerminalUi::print_ticket_stub(stub_out, "A1", 3);
    std::string stub_str = stub_out.str();
    assert(stub_str.find("ADMIT ONE") != std::string::npos);
    assert(stub_str.find("A1") != std::string::npos);
    assert(stub_str.find("CLIENT #") != std::string::npos);
}

void test_client_queue_naming_and_state() {
    css223::client::Client client(3);
    assert(client.client_id() == 3);
    assert(client.reply_queue_name() == "/css223_client_3_reply");
    assert(!client.is_connected());
}

void test_client_repl_stream_execution() {
    css223::client::Client client(9);
    css223::client::ClientRepl repl(client);

    std::istringstream mock_in("HELP\nUNKNOWN_SYNTAX\nQUIT\n");
    std::ostringstream mock_out;

    repl.run(mock_in, mock_out);

    std::string output = mock_out.str();
    assert(output.find("WELCOME TO CSS223 CINEMA THEATER") != std::string::npos);
    assert(output.find("Connected to Box Office as Client ID: 9") != std::string::npos);
    assert(output.find("Box Office Counter") != std::string::npos);
    assert(output.find("LIST") != std::string::npos);
    assert(output.find("STATUS <seat_id>") != std::string::npos);
    assert(output.find("Unknown command syntax") != std::string::npos);
    assert(output.find("Leaving Cinema Theater. Reply queue unlinked. See you next show!") !=
           std::string::npos);
}

} // namespace

int main() {
    test_command_parser_valid_commands();
    test_command_parser_invalid_commands();
    test_seat_map_formatter();
    test_terminal_ui_helpers();
    test_client_queue_naming_and_state();
    test_client_repl_stream_execution();

    return EXIT_SUCCESS;
}
