#include "client/terminal_ui.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(__linux__) || defined(__unix__)
    #include <fcntl.h>
    #include <sys/ioctl.h>
    #include <unistd.h>
#endif

#include "client/seat_map_formatter.hpp"
#include "common/constants.hpp"
#include "common/types.hpp"
#include "core/seat.hpp"

namespace css223::client {

bool TerminalUi::is_interactive() noexcept {
    const char* force = std::getenv("CSS223_FORCE_COLOR");
    if (force != nullptr && (std::string_view(force) == "1" || std::string_view(force) == "true")) {
        return true;
    }
#if defined(__linux__) || defined(__unix__)
    return ::isatty(STDOUT_FILENO) != 0;
#else
    return false;
#endif
}

int TerminalUi::get_terminal_width() noexcept {
#if defined(__linux__) || defined(__unix__)
    struct winsize window_size{};
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size) == 0 && window_size.ws_col >= 40) {
        return static_cast<int>(window_size.ws_col);
    }
    if (::ioctl(STDIN_FILENO, TIOCGWINSZ, &window_size) == 0 && window_size.ws_col >= 40) {
        return static_cast<int>(window_size.ws_col);
    }
    int tty_fd = ::open("/dev/tty", O_RDONLY | O_NOCTTY);
    if (tty_fd >= 0) {
        if (::ioctl(tty_fd, TIOCGWINSZ, &window_size) == 0 && window_size.ws_col >= 40) {
            ::close(tty_fd);
            return static_cast<int>(window_size.ws_col);
        }
        ::close(tty_fd);
    }
#endif

    const char* env_col = std::getenv("COLUMNS");
    if (env_col != nullptr) {
        int parsed = std::atoi(env_col);
        if (parsed >= 40) {
            return parsed;
        }
    }

    return 120; // default widescreen width
}

std::size_t TerminalUi::visual_width(std::string_view line) noexcept {
    std::size_t width = 0;
    std::size_t i = 0;
    while (i < line.size()) {
        if (line[i] == '\033') {
            ++i;
            if (i < line.size() && line[i] == '[') {
                ++i;
                while (i < line.size() && (line[i] < '@' || line[i] > '~')) {
                    ++i;
                }
                if (i < line.size()) {
                    ++i;
                }
            }
            continue;
        }

        auto c = static_cast<unsigned char>(line[i]);
        if (c < 0x80) {
            ++width;
            ++i;
        } else if ((c & 0xE0) == 0xC0) {
            ++width;
            i += std::min<std::size_t>(2, line.size() - i);
        } else if ((c & 0xF0) == 0xE0) {
            ++width;
            i += std::min<std::size_t>(3, line.size() - i);
        } else if ((c & 0xF8) == 0xF0) {
            width += 2;
            i += std::min<std::size_t>(4, line.size() - i);
        } else {
            ++i;
        }
    }
    return width;
}

namespace {

volatile std::sig_atomic_t g_terminal_resized = 0;

#if defined(__linux__) || defined(__unix__)
void sigwinch_handler(int) {
    g_terminal_resized = 1;
}
#endif

} // namespace

void TerminalUi::init_signal_handlers() noexcept {
#if defined(__linux__) || defined(__unix__)
    struct sigaction sa{};
    sa.sa_handler = sigwinch_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGWINCH, &sa, nullptr);
#endif
}

bool TerminalUi::has_resized() noexcept {
    return g_terminal_resized != 0;
}

void TerminalUi::reset_resized() noexcept {
    g_terminal_resized = 0;
}

std::string TerminalUi::get_padding(int width) noexcept {
    if (!is_interactive()) {
        return "";
    }
    if (width <= 0) {
        width = get_terminal_width();
    }
    if (static_cast<std::size_t>(width) > kStandardBlockWidth) {
        return std::string((static_cast<std::size_t>(width) - kStandardBlockWidth) / 2, ' ');
    }
    return "";
}

std::string TerminalUi::sanitize_input(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    std::size_t i = 0;
    while (i < input.size()) {
        if (input[i] == '\033' || input[i] == '\x1b') {
            ++i;
            if (i < input.size() && input[i] == '[') {
                ++i;
                while (i < input.size() &&
                       (std::isdigit(static_cast<unsigned char>(input[i])) || input[i] == ';' ||
                        input[i] == '?' || input[i] == ' ')) {
                    ++i;
                }
                if (i < input.size()) {
                    ++i;
                }
            } else if (i < input.size() && input[i] == 'O') {
                i += 2;
            }
            continue;
        }
        if (input[i] == '\r') {
            ++i;
            continue;
        }
        result.push_back(input[i]);
        ++i;
    }
    std::size_t start = 0;
    while (start < result.size() && std::isspace(static_cast<unsigned char>(result[start])) != 0) {
        ++start;
    }
    std::size_t end = result.size();
    while (end > start && std::isspace(static_cast<unsigned char>(result[end - 1])) != 0) {
        --end;
    }
    return result.substr(start, end - start);
}

std::string TerminalUi::center_line(std::string_view line, int width) {
    if (!is_interactive() || line.empty()) {
        return std::string(line);
    }
    return get_padding(width) + std::string(line);
}

std::string TerminalUi::center_block(std::string_view block, int width) {
    if (!is_interactive() || block.empty()) {
        return std::string(block);
    }
    std::string pad_str = get_padding(width);
    std::ostringstream result;
    std::size_t start = 0;
    while (start < block.size()) {
        std::size_t end = block.find('\n', start);
        if (end == std::string_view::npos) {
            end = block.size();
        }
        std::string_view line = block.substr(start, end - start);
        if (!line.empty()) {
            result << pad_str << line;
        }
        if (end < block.size()) {
            result << '\n';
        }
        start = end + 1;
    }
    return result.str();
}

void TerminalUi::clear_screen(std::ostream& out) {
    if (is_interactive()) {
        out << "\033[2J\033[H\033[3J";
        out.flush();
    }
}

void TerminalUi::animate_ticket_print(std::ostream& out,
                                      std::string_view seat_id,
                                      common::ClientId client_id) {
    if (!is_interactive()) {
        return;
    }
    std::ostringstream ticket;
    ticket << colors::kBrandLine
           << "┌─ RESERVATION COMMIT ───────────────────────────────────────────────────────────┐\n"
           << "│  " << colors::kBrandBlue << colors::kBold << "WRITE" << colors::kReset
           << colors::kBrandWhite << "  seat " << seat_id << " → client " << client_id
           << colors::kBrandMuted << "  / validating mutex-protected state...                    "
           << colors::kBrandLine << "│\n"
           << "└───────────────────────────────────────────────────────────────────────────────┘"
           << colors::kReset;
    out << "\n" << center_block(ticket.str()) << "\n";
    out.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
}

void TerminalUi::print_ticket_stub(std::ostream& out,
                                   std::string_view seat_id,
                                   common::ClientId client_id) {
    bool interactive = is_interactive();
    char row_char = seat_id.empty() ? 'A' : seat_id.front();

    std::ostringstream ss;
    if (interactive) {
        int width = get_terminal_width();
        std::string pad = get_padding(width);
        ss << pad << colors::kBrandLine
           << "┌─ CSS223 / RESERVATION RECEIPT "
              "──────────────────────────────────────────────────────────────┐\n"
           << pad << "│  " << colors::kBrandBlue << colors::kBold << "🎬  CSS223 CINEMA THEATER"
           << colors::kReset << colors::kBrandMuted
           << "                                             ★ " << colors::kBrandWarning
           << "ADMIT ONE" << colors::kBrandMuted << " ★        " << colors::kBrandLine << "│\n"
           << pad
           << "├───────────────────────────────────────────────────────────────────────────────────"
              "───────────┤\n"
           << pad << "│  " << colors::kBrandMuted << "FEATURE   : " << colors::kBrandWhite
           << colors::kBold << std::left << std::setw(42) << "CONCURRENT CINEMA: THE THREAD OF FATE"
           << colors::kReset << colors::kBrandLine << "│ " << colors::kBrandMuted
           << "AUDITORIUM : " << colors::kBrandWhite << colors::kBold << std::left << std::setw(15)
           << "IMAX 01" << colors::kReset << colors::kBrandLine << "│\n"
           << pad << "│  " << colors::kBrandMuted << "SEAT      : " << colors::kBrandWarning
           << colors::kBold << std::left << std::setw(42)
           << (std::string(seat_id) + " (DOLBY LASER PREMIUM)") << colors::kReset
           << colors::kBrandLine << "│ " << colors::kBrandMuted
           << "ROW        : " << colors::kBrandWhite << colors::kBold << std::left << std::setw(15)
           << std::string(1, row_char) << colors::kReset << colors::kBrandLine << "│\n"
           << pad << "│  " << colors::kBrandMuted << "OWNER     : " << colors::kBrandWhite
           << colors::kBold << std::left << std::setw(42)
           << ("CLIENT #" + std::to_string(client_id) + " (POSIX MQ)") << colors::kReset
           << colors::kBrandLine << "│ " << colors::kBrandMuted
           << "SYNC       : " << colors::kBrandBlueSoft << colors::kBold << std::left
           << std::setw(15) << "MUTEX VERIFIED" << colors::kReset << colors::kBrandLine << "│\n"
           << pad << "│  " << colors::kBrandMuted << "PRICE     : " << colors::kBrandSuccess
           << colors::kBold << std::left << std::setw(42) << "฿240.00 (PAID • SYSTEM SYNCHRONIZED)"
           << colors::kReset << colors::kBrandLine << "│ " << colors::kBrandMuted
           << "STATUS     : " << colors::kBrandSuccess << colors::kBold << std::left
           << std::setw(15) << "BOOKED" << colors::kReset << colors::kBrandLine << "│\n"
           << pad << colors::kBrandLine
           << "└───────────────────────────────────────────────────────────────────────────────────"
              "───────────┘\n"
           << colors::kReset;
    } else {
        ss << ".───────────────────────────────────────────────────────────────────────────────────"
              "───.\n"
           << "│  🎬  C S S 2 2 3   C I N E M A   T H E A T E R                     ★ ADMIT ONE ★  "
              "   │\n"
           << "├──────────────────────────────────────────────────────────────┬────────────────────"
              "───┤\n"
           << "│  FEATURE : CONCURRENT CINEMA: THE THREAD OF FATE             │  AUDITORIUM : IMAX "
              "01 │\n"
           << "│  SEAT    : " << std::left << std::setw(4) << seat_id
           << " (DOLBY LASER PREMIUM RESERVED)           │  ROW        : " << row_char
           << "       │\n"
           << "│  OWNER   : CLIENT #" << std::left << std::setw(2) << client_id
           << " (POSIX MESSAGE QUEUE VERIFIED)       │  SOUND      : ATMOS   │\n"
           << "│  PRICE   : ฿240.00 (PAID • SYSTEM SYNCHRONIZED)          │  STATUS     : BOOKED  "
              "│\n"
           << "'──────────────────────────────────────────────────────────────┴────────────────────"
              "───'";
    }

    out << "\n" << (interactive ? center_block(ss.str()) : ss.str()) << "\n";
    out.flush();
}

void TerminalUi::show_welcome_banner(std::ostream& out, common::ClientId client_id, bool animate) {
    bool interactive = is_interactive();
    bool should_animate = animate && interactive;

    if (should_animate) {
        clear_screen(out);

        // Short, deterministic boot sequence inspired by INTECH's precise visual language.
        const std::vector<std::string_view> kProjectorFrames = {
            "01 / BOOT     POSIX MESSAGE QUEUE",
            "02 / VERIFY   MUTEX-PROTECTED STATE",
            "03 / READY    CONCURRENT RESERVATION SYSTEM",
        };

        for (const auto& frame : kProjectorFrames) {
            out << "\033[2K\r"
                << center_line(std::string(colors::kBold) + std::string(colors::kBrandBlue) +
                               std::string(frame) + std::string(colors::kReset));
            out.flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(70));
        }
        out << "\033[2K\r";
        out.flush();
    }

    if (interactive) {
        std::ostringstream hero;
        hero << colors::kBrandLine
             << "┌─────────────────────────────────────────────────────────────────────────────────"
                "─────────────┐\n"
             << "│ " << colors::kBrandWhite << colors::kBold << "INTECH / CSS223" << colors::kReset
             << colors::kBrandMuted
             << "                                  POSIX MQ  ·  C++17  ·  MUTEX  ·  LINUX "
             << colors::kBrandLine << "│\n"
             << "├─────────────────────────────────────────────────────────────────────────────────"
                "─────────────┤\n"
             << "│ " << colors::kBrandBlue << colors::kBold
             << "■──□──■   CONCURRENCY, WITHOUT COLLISIONS." << colors::kReset << colors::kBrandLine
             << "                                             │\n"
             << "│ " << colors::kBrandWhite << colors::kBold << "WELCOME TO CSS223 CINEMA THEATER"
             << colors::kReset << colors::kBrandMuted
             << "   Reliable reservations. Clear ownership. No double booking. "
             << colors::kBrandLine << "│\n"
             << "└─────────────────────────────────────────────────────────────────────────────────"
                "─────────────┘"
             << colors::kReset;
        out << center_block(hero.str()) << "\n\n";
    } else {
        out << "\n"
            << "INTECH / CSS223\n"
            << "CONCURRENCY, WITHOUT COLLISIONS.\n"
            << "WELCOME TO CSS223 CINEMA THEATER\n"
            << "Multi-Threaded Server & Concurrent Client System\n\n";
    }

    show_help_box(out);

    std::string conn_str = "Connected to Box Office as Client ID: " + std::to_string(client_id);
    if (interactive) {
        out << "\n"
            << center_line(std::string(colors::kBrandMuted) +
                           "SESSION / Connected to Box Office as Client ID: " +
                           std::string(colors::kReset) + std::string(colors::kBold) +
                           std::string(colors::kBrandBlueSoft) + std::to_string(client_id) +
                           std::string(colors::kReset))
            << "\n\n";
    } else {
        out << conn_str << "\n\n";
    }
}

void TerminalUi::show_help_box(std::ostream& out) {
    bool interactive = is_interactive();

    if (interactive) {
        std::ostringstream ss;
        ss << colors::kBrandLine
           << "┌─ 01 / BOX OFFICE COUNTER · COMMANDS "
              "────────────────────────────────────────────────────────┐\n"
           << "│  " << colors::kBrandBlue << colors::kBold << "[1] LIST" << colors::kReset
           << colors::kBrandMuted << "  all seats        " << colors::kBrandBlueSoft
           << colors::kBold << "[2] RESERVE <seat>" << colors::kReset << colors::kBrandMuted
           << "  book        " << colors::kBrandBlueSoft << colors::kBold << "[3] STATUS <seat>"
           << colors::kReset << colors::kBrandMuted << "  inspect   " << colors::kBrandLine << "│\n"
           << "│  " << colors::kBrandDanger << colors::kBold << "[4] CANCEL <seat>"
           << colors::kReset << colors::kBrandMuted << "  release   " << colors::kBrandWhite
           << colors::kBold << "[5] HELP" << colors::kReset << colors::kBrandMuted << "  guide   "
           << colors::kBrandWhite << colors::kBold << "[6] QUIT" << colors::kReset
           << colors::kBrandMuted << "  exit   " << colors::kBrandWhite << colors::kBold
           << "[7] CLEAR" << colors::kReset << colors::kBrandMuted << "  refresh                 "
           << colors::kBrandLine << "│\n"
           << "│  " << colors::kBrandMuted
           << "Shortcuts: 1  ·  2 A1  ·  3 A1  ·  4 A1   /   direct commands are case-insensitive. "
              "          "
           << colors::kBrandLine << "│\n"
           << "└───────────────────────────────────────────────────────────────────────────────────"
              "──────────┘\n"
           << colors::kReset;
        out << center_block(ss.str());
    } else {
        out << "  +------------------------------------------------------------------+\n"
            << "  | Box Office Counter • Select an option to proceed:                |\n"
            << "  |                                                                  |\n"
            << "  | > LIST              View all 20 cinema seats & current status    |\n"
            << "  |   STATUS <seat_id>  Check seat status (e.g. STATUS A1)           |\n"
            << "  |   RESERVE <seat_id> Book your cinema seat (e.g. RESERVE A1)      |\n"
            << "  |   CANCEL <seat_id>  Cancel reservation (e.g. CANCEL A1)          |\n"
            << "  |   QUIT              Leave cinema & disconnect session            |\n"
            << "  |   HELP              Show box office instructions                 |\n"
            << "  |   CLEAR / CLS       Refresh & re-center Cinema Theater screen    |\n"
            << "  |                                                                  |\n"
            << "  | Tip: Type a command below and press Enter to begin!              |\n"
            << "  +------------------------------------------------------------------+\n";
    }
}

std::string TerminalUi::format_prompt(common::ClientId client_id, bool colorize) {
    std::ostringstream ss;
    if (colorize && is_interactive()) {
        int width = get_terminal_width();
        std::string pad_str = get_padding(width);
        std::string client_str = std::to_string(client_id);
        std::size_t dashes_count = (kStandardBlockWidth > (20 + client_str.size()))
                                       ? kStandardBlockWidth - 20 - client_str.size()
                                       : 10;
        std::string dashes_str;
        for (std::size_t i = 0; i < dashes_count; ++i) {
            dashes_str += "─";
        }
        ss << pad_str << colors::kBrandLine << "┌─ INPUT / CLIENT " << client_str << " "
           << dashes_str << "╮\n"
           << pad_str << "└─❯ " << colors::kReset << colors::kBold << colors::kBrandBlue;
    } else {
        ss << "Client[" << client_id << "]> ";
    }
    return ss.str();
}

std::string TerminalUi::format_success(std::string_view message, bool colorize) {
    std::ostringstream ss;
    if (colorize && is_interactive()) {
        ss << colors::kBold << colors::kBrandSuccess << "  [SUCCESS] " << colors::kReset
           << colors::kBrandWhite << message << colors::kReset;
        return center_line(ss.str());
    }
    ss << "  [SUCCESS] " << message;
    return ss.str();
}

std::string TerminalUi::format_error(std::string_view message, bool colorize) {
    std::ostringstream ss;
    if (colorize && is_interactive()) {
        ss << colors::kBold << colors::kBrandDanger << "  [FAILED] " << colors::kReset
           << colors::kBrandWhite << message << colors::kReset;
        return center_line(ss.str());
    }
    ss << "  [FAILED] " << message;
    return ss.str();
}

std::string TerminalUi::format_info(std::string_view message, bool colorize) {
    std::ostringstream ss;
    if (colorize && is_interactive()) {
        ss << colors::kBrandBlueSoft << "  [INFO] " << colors::kReset << colors::kBrandWhite
           << message << colors::kReset;
        return center_line(ss.str());
    }
    ss << "  [INFO] " << message;
    return ss.str();
}

std::string TerminalUi::format_grid(const std::vector<SeatDisplayInfo>& seats,
                                    bool colorize,
                                    common::ClientId current_client_id) {
    bool use_color = colorize && is_interactive();
    std::ostringstream out;

    if (use_color) {
        out << "\n"
            << colors::kBrandLine
            << "┌─ 02 / AUDITORIUM "
               "────────────────────────────────────────────────────────────────────────────┐\n"
            << "│ " << colors::kBrandBlue << colors::kBold
            << "■────────□────────■        C I N E M A   S C R E E N        ■────────□────────■"
            << colors::kReset << colors::kBrandLine << "              │\n"
            << "│ " << colors::kBrandMuted
            << "                         DOLBY VISION  ·  4K DUAL LASER  ·  ATMOS                  "
               "           "
            << colors::kBrandLine << "│\n"
            << "└──────────────────────────────────────────────────────────────────────────────────"
               "────────────┘\n"
            << colors::kBrandMuted << "  TIERS   " << colors::kBrandWarning << "A / VIP ฿320"
            << colors::kReset << colors::kBrandMuted << "  ·  " << colors::kBrandBlueSoft
            << "B / PREM ฿280" << colors::kReset << colors::kBrandMuted << "  ·  "
            << colors::kBrandWhite << "C-D / STD ฿240" << colors::kReset << colors::kBrandMuted
            << "    STATUS   " << colors::kBrandSuccess << "■ FREE" << colors::kReset
            << colors::kBrandMuted << "  " << colors::kBrandWarning << "■ MINE" << colors::kReset
            << colors::kBrandMuted << "  " << colors::kBrandDanger << "■ BOOKED" << colors::kReset
            << "\n\n"
            << colors::kReset;
    } else {
        out << "\n"
            << "  +------------------------------------------------------------------+\n"
            << "  |                  ======== CINEMA SCREEN ========                 |\n"
            << "  +------------------------------------------------------------------+\n\n";
    }

    std::size_t available_count = 0;
    std::size_t reserved_count = 0;
    char current_row = '\0';

    for (const auto& seat : seats) {
        if (seat.seat_id.empty()) {
            continue;
        }

        if (seat.status == core::SeatStatus::Available) {
            ++available_count;
        } else {
            ++reserved_count;
        }

        char row_char = seat.seat_id.front();
        if (row_char != current_row) {
            if (current_row != '\0') {
                out << "\n";
            }
            current_row = row_char;
            if (use_color) {
                if (current_row == 'A') {
                    out << "    " << colors::kBold << colors::kBrandWarning << "A / VIP       "
                        << colors::kReset;
                } else if (current_row == 'B') {
                    out << "    " << colors::kBold << colors::kBrandBlueSoft << "B / PREMIER   "
                        << colors::kReset;
                } else {
                    out << "    " << colors::kBold << colors::kBrandWhite << current_row
                        << " / STANDARD  " << colors::kReset;
                }
            } else {
                out << "  Row " << current_row << ":  ";
            }
        } else {
            out << (use_color ? "  " : " ");
        }

        std::ostringstream token;
        if (use_color) {
            std::string_view tier_color = colors::kBrandWhite;
            if (current_row == 'A') {
                tier_color = colors::kBrandWarning;
            } else if (current_row == 'B') {
                tier_color = colors::kBrandBlueSoft;
            }

            if (seat.status == core::SeatStatus::Available) {
                token << colors::kBrandLine << "[ " << colors::kReset << colors::kBold << tier_color
                      << std::left << std::setw(2) << seat.seat_id << colors::kReset
                      << colors::kBrandLine << " / " << colors::kReset << colors::kBrandSuccess
                      << colors::kBold << "FREE " << colors::kReset << colors::kBrandLine << "]"
                      << colors::kReset;
            } else if (current_client_id != common::kInvalidClientId &&
                       seat.owner_client_id == current_client_id) {
                token << colors::kBrandWarning << "[ " << colors::kBold << std::left << std::setw(2)
                      << seat.seat_id << " / MINE " << colors::kReset << colors::kBrandWarning
                      << "]" << colors::kReset;
            } else {
                token << colors::kBrandDanger << "[ " << colors::kBold << std::left << std::setw(2)
                      << seat.seat_id << " / C" << std::setfill('0') << std::right << std::setw(2)
                      << seat.owner_client_id << "  " << colors::kReset << colors::kBrandDanger
                      << "]" << colors::kReset << std::setfill(' ') << std::left;
            }
            out << token.str();
        } else {
            token << "[" << seat.seat_id << ": ";
            if (seat.status == core::SeatStatus::Available) {
                token << "AVAIL    ]";
            } else {
                token << "RSV (C#" << seat.owner_client_id << ")]";
            }
            out << std::left << std::setw(14) << token.str();
        }
    }

    out << "\n\n";

    // Occupancy Progress Bar Calculation
    std::size_t total_seats = available_count + reserved_count;
    double occupancy_pct =
        (total_seats > 0)
            ? (static_cast<double>(reserved_count) / static_cast<double>(total_seats)) * 100.0
            : 0.0;

    constexpr std::size_t kBarWidth = 40;
    std::size_t filled_segments = static_cast<std::size_t>(
        std::max(0L, std::lround((occupancy_pct / 100.0) * static_cast<double>(kBarWidth))));
    filled_segments = std::min(filled_segments, kBarWidth);
    std::size_t empty_segments = kBarWidth - filled_segments;

    std::string filled_bar;
    for (std::size_t i = 0; i < filled_segments; ++i) {
        filled_bar += "█";
    }
    std::string empty_bar;
    for (std::size_t i = 0; i < empty_segments; ++i) {
        empty_bar += "░";
    }

    std::string_view bar_color = colors::kBrandBlue;
    if (occupancy_pct >= 85.0) {
        bar_color = colors::kBrandDanger;
    } else if (occupancy_pct >= 60.0) {
        bar_color = colors::kBrandWarning;
    }

    if (use_color) {
        out << colors::kBrandLine
            << "───────────────────────────────────────────────────────────────────────────────────"
               "────────────\n"
            << colors::kBrandMuted << "  CAPACITY  " << colors::kReset << colors::kBrandWhite
            << total_seats << " seats" << colors::kReset << colors::kBrandMuted << "   ·   "
            << colors::kBrandSuccess << available_count << " available" << colors::kReset
            << colors::kBrandMuted << "   ·   " << colors::kBrandWarning << reserved_count
            << " reserved" << colors::kReset << colors::kBrandMuted << "   ·   "
            << colors::kBrandBlueSoft << "mutex synchronized" << colors::kReset << "\n"
            << colors::kBrandMuted << "  LOAD      [" << colors::kReset << bar_color << filled_bar
            << colors::kReset << colors::kBrandLine << empty_bar << colors::kBrandMuted << "]  "
            << colors::kReset << colors::kBold << colors::kBrandWhite << std::fixed
            << std::setprecision(1) << occupancy_pct << "%" << colors::kReset << colors::kBrandMuted
            << "  (" << reserved_count << "/" << total_seats << " booked)\n"
            << colors::kBrandLine
            << "───────────────────────────────────────────────────────────────────────────────────"
               "────────────\n"
            << colors::kReset;
    } else {
        out << "  --------------------------------------------------------------------\n"
            << "  Box Office: Total = " << total_seats << " | Available = " << available_count
            << " | Reserved = " << reserved_count << "\n"
            << "  THEATER CAPACITY : [" << filled_bar << empty_bar << "]  " << std::fixed
            << std::setprecision(1) << occupancy_pct << "%  (" << reserved_count << "/"
            << total_seats << " Booked)\n"
            << "  --------------------------------------------------------------------\n";
    }

    if (use_color) {
        return center_block(out.str());
    }
    return out.str();
}

void TerminalUi::render_kiosk_view(std::ostream& out,
                                   const std::vector<SeatDisplayInfo>& seats,
                                   common::ClientId client_id,
                                   std::string_view feedback_msg,
                                   std::string_view feedback_type,
                                   std::string_view ticket_seat_id) {
    if (!is_interactive()) {
        return;
    }

    clear_screen(out);

    int width = get_terminal_width();

    // 1. Hero Banner
    std::ostringstream hero;
    hero << colors::kBrandLine
         << "┌─────────────────────────────────────────────────────────────────────────────────────"
            "─────────┐\n"
         << "│ " << colors::kBrandWhite << colors::kBold << "INTECH / CSS223" << colors::kReset
         << colors::kBrandMuted
         << "                                  POSIX MQ  ·  C++17  ·  MUTEX  ·  LINUX      "
         << colors::kBrandLine << "│\n"
         << "├─────────────────────────────────────────────────────────────────────────────────────"
            "─────────┤\n"
         << "│ " << colors::kBrandBlue << colors::kBold
         << "■──□──■   CONCURRENCY, WITHOUT COLLISIONS." << colors::kReset << colors::kBrandLine
         << "                                             │\n"
         << "│ " << colors::kBrandWhite << colors::kBold << "WELCOME TO CSS223 CINEMA THEATER"
         << colors::kReset << colors::kBrandMuted
         << "   Reliable reservations. Clear ownership. No double booking. " << colors::kBrandLine
         << "│\n"
         << "└─────────────────────────────────────────────────────────────────────────────────────"
            "─────────┘\n"
         << colors::kReset;
    out << center_block(hero.str(), width) << "\n";

    // 2. Help Box / Commands Menu
    std::ostringstream help;
    help << colors::kBrandLine
         << "┌─ 01 / BOX OFFICE COUNTER · COMMANDS "
            "─────────────────────────────────────────────────────────┐\n"
         << "│  " << colors::kBrandBlue << colors::kBold << "[1] LIST" << colors::kReset
         << colors::kBrandMuted << "  all seats        " << colors::kBrandBlueSoft << colors::kBold
         << "[2] RESERVE <seat>" << colors::kReset << colors::kBrandMuted << "  book        "
         << colors::kBrandBlueSoft << colors::kBold << "[3] STATUS <seat>" << colors::kReset
         << colors::kBrandMuted << "  inspect    " << colors::kBrandLine << "│\n"
         << "│  " << colors::kBrandDanger << colors::kBold << "[4] CANCEL <seat>" << colors::kReset
         << colors::kBrandMuted << "  release    " << colors::kBrandWhite << colors::kBold
         << "[5] HELP" << colors::kReset << colors::kBrandMuted << "  guide   "
         << colors::kBrandWhite << colors::kBold << "[6] QUIT" << colors::kReset
         << colors::kBrandMuted << "  exit   " << colors::kBrandWhite << colors::kBold
         << "[7] CLEAR" << colors::kReset << colors::kBrandMuted << "  refresh                  "
         << colors::kBrandLine << "│\n"
         << "│  " << colors::kBrandMuted
         << "Shortcuts: 1  ·  2 A1  ·  3 A1  ·  4 A1   /   direct commands are case-insensitive.   "
            "       "
         << colors::kBrandLine << "│\n"
         << "└─────────────────────────────────────────────────────────────────────────────────────"
            "─────────┘\n"
         << colors::kReset;
    out << center_block(help.str(), width) << "\n";

    // 3. Auditorium Seat Grid
    out << format_grid(seats, true, client_id) << "\n";

    // 4. Ticket stub or feedback line
    if (!ticket_seat_id.empty()) {
        print_ticket_stub(out, ticket_seat_id, client_id);
        out << "\n";
    }

    if (!feedback_msg.empty()) {
        if (feedback_type == "SUCCESS") {
            out << format_success(feedback_msg) << "\n\n";
        } else if (feedback_type == "FAILED") {
            out << format_error(feedback_msg) << "\n\n";
        } else {
            out << format_info(feedback_msg) << "\n\n";
        }
    }

    // 5. Input Prompt Box
    out << format_prompt(client_id, true);
    out.flush();
}

} // namespace css223::client
