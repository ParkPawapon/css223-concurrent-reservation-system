#include "client/terminal_ui.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
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
#if defined(__linux__) || defined(__unix__)
    return ::isatty(STDOUT_FILENO) != 0;
#else
    return false;
#endif
}

int TerminalUi::get_terminal_width() noexcept {
    const char* env_col = std::getenv("COLUMNS");
    if (env_col != nullptr) {
        int parsed = std::atoi(env_col);
        if (parsed >= 40) {
            return parsed;
        }
    }

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

std::string TerminalUi::center_line(std::string_view line, int width) {
    if (!is_interactive() || line.empty()) {
        return std::string(line);
    }
    if (width <= 0) {
        width = get_terminal_width();
    }
    std::size_t vwidth = visual_width(line);
    if (static_cast<int>(vwidth) >= width) {
        return std::string(line);
    }
    std::size_t pad = (static_cast<std::size_t>(width) - vwidth) / 2;
    return std::string(pad, ' ') + std::string(line);
}

std::string TerminalUi::center_block(std::string_view block, int width) {
    if (!is_interactive() || block.empty()) {
        return std::string(block);
    }
    if (width <= 0) {
        width = get_terminal_width();
    }

    std::size_t max_width = 0;
    std::size_t start = 0;
    while (start < block.size()) {
        std::size_t end = block.find('\n', start);
        if (end == std::string_view::npos) {
            end = block.size();
        }
        std::string_view line = block.substr(start, end - start);
        std::size_t vw = visual_width(line);
        max_width = std::max(max_width, vw);
        start = end + 1;
    }

    std::size_t pad = 0;
    if (static_cast<std::size_t>(width) > max_width) {
        pad = (static_cast<std::size_t>(width) - max_width) / 2;
    }

    std::string pad_str(pad, ' ');
    std::ostringstream result;
    start = 0;
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
    ticket << colors::kBold << colors::kBrightYellow
           << "╔═══════════════════════════════════════════════════════════════════════════════════"
              "═══╗\n"
           << "║  🎟️  DISPENSING CINEMA TICKET: Seat " << seat_id << " for Client #" << client_id
           << "  •  DOLBY LASER PASS   ║\n"
           << "╚═══════════════════════════════════════════════════════════════════════════════════"
              "═══╝"
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
        ss << colors::kBrightYellow
           << ".───────────────────────────────────────────────────────────────────────────────────"
              "───.\n"
           << "│ " << colors::kReset << colors::kBold << colors::kBrightYellow
           << " 🎬  C S S 2 2 3   C I N E M A   T H E A T E R                     ★ ADMIT ONE ★    "
              " "
           << colors::kReset << colors::kBrightYellow << "│\n"
           << "├──────────────────────────────────────────────────────────────┬────────────────────"
              "───┤\n"
           << "│ " << colors::kReset << colors::kDim << "FEATURE : " << colors::kReset
           << colors::kBold << "CONCURRENT CINEMA: THE THREAD OF FATE            "
           << colors::kBrightYellow << "│ " << colors::kReset << colors::kDim
           << "AUDITORIUM : " << colors::kReset << colors::kCyan << "IMAX 01 "
           << colors::kBrightYellow << "│\n"
           << "│ " << colors::kReset << colors::kDim << "SEAT    : " << colors::kReset
           << colors::kBold << colors::kBrightGreen << std::left << std::setw(4) << seat_id
           << colors::kReset << colors::kDim << "(DOLBY LASER PREMIUM RESERVED)           "
           << colors::kBrightYellow << "│ " << colors::kReset << colors::kDim
           << "ROW        : " << colors::kReset << colors::kBold << row_char << "       "
           << colors::kBrightYellow << "│\n"
           << "│ " << colors::kReset << colors::kDim << "OWNER   : " << colors::kReset
           << colors::kBold << colors::kBrightYellow << "CLIENT #" << std::left << std::setw(2)
           << client_id << colors::kReset << colors::kDim
           << " (POSIX MESSAGE QUEUE VERIFIED)       " << colors::kBrightYellow << "│ "
           << colors::kReset << colors::kDim << "SOUND      : " << colors::kReset << colors::kCyan
           << "ATMOS   " << colors::kBrightYellow << "│\n"
           << "│ " << colors::kReset << colors::kDim << "PRICE   : " << colors::kReset
           << "฿240.00 (PAID • SYSTEM SYNCHRONIZED)          " << colors::kBrightYellow << "│ "
           << colors::kReset << colors::kDim << "STATUS     : " << colors::kReset
           << colors::kBrightGreen << "BOOKED  " << colors::kBrightYellow << "│\n"
           << "'──────────────────────────────────────────────────────────────┴────────────────────"
              "───'"
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

        // Animation: Film Reel Projector Countdown
        const std::vector<std::string_view> kProjectorFrames = {
            "[ 📽️  FILM REEL: 🎞️  •  •  •  • ]   THREADING 35MM CINEMA FILM...",
            "[ 📽️  FILM REEL: •  🎞️  •  •  • ]   POWERING DOLBY ATMOS AUDIO...",
            "[ 📽️  FILM REEL: •  •  🎞️  •  • ]   CALIBRATING 4K LASER IMAX...",
            "[ 📽️  FILM REEL: •  •  •  •  🎞️ ]   CURTAIN OPENING! ACTION!",
        };

        for (const auto& frame : kProjectorFrames) {
            out << "\033[2K\r"
                << center_line(std::string(colors::kBold) + std::string(colors::kBrightCyan) +
                               std::string(frame) + std::string(colors::kReset));
            out.flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
        }
        out << "\033[2K\r"; // Clear line cleanly
        out.flush();
    }

    // Authentic 88-Column Clapperboard
    const std::string kClapperSnap =
        "                     *  *  *   N O W   S H O W I N G   *  *  *\n"
        "       .========================================================================.\n"
        "       |   //    //    //    //    //    //    //    //    //    //    //    //  |\n"
        "       |___//____//____//____//____//____//____//____//____//____//____//____//__|\n"
        "       +========================================================================+\n"
        "       |  🎬  * C L A P ! *               |  SCENE: 01       |  TAKE: 01 ACTION |\n"
        "       |  TITLE: CSS223 CONCURRENT CINEMA |  DATE : 2026     |  FPS : 24 RAW    |\n"
        "       |  SYNC : MUTEX (EXP 1, 2, 3)      |  IPC  : POSIX MQ |  OS  : LINUX C17 |\n"
        "       +========================================================================+";

    if (interactive) {
        out << center_block(kClapperSnap) << "\n\n";

        // Marquee
        std::string title_line1 = "★  ★  ★   N O W   S H O W I N G   ★  ★  ★";
        std::string title_line2 = "WELCOME TO CSS223 CINEMA THEATER";
        std::string title_line3 = "Multi-Threaded Server & Concurrent Client System";

        out << center_line(std::string(colors::kBrightYellow) + title_line1 +
                           std::string(colors::kReset))
            << "\n"
            << center_line(std::string(colors::kBold) + std::string(colors::kBrightGreen) +
                           title_line2 + std::string(colors::kReset))
            << "\n"
            << center_line(std::string(colors::kDim) + title_line3 + std::string(colors::kReset))
            << "\n\n";
    } else {
        out << "\n"
            << kClapperSnap << "\n\n"
            << "               ★  ★  ★   NOW SHOWING   ★  ★  ★\n"
            << "               WELCOME TO CSS223 CINEMA THEATER\n"
            << "       Multi-Threaded Server & Concurrent Client System\n\n";
    }

    show_help_box(out);

    std::string conn_str = "Connected to Box Office as Client ID: " + std::to_string(client_id);
    if (interactive) {
        out << "\n"
            << center_line(std::string(colors::kDim) +
                           "Connected to Box Office as Client ID: " + std::string(colors::kReset) +
                           std::string(colors::kBold) + std::string(colors::kBrightYellow) +
                           std::to_string(client_id) + std::string(colors::kReset))
            << "\n\n";
    } else {
        out << conn_str << "\n\n";
    }
}

void TerminalUi::show_help_box(std::ostream& out) {
    bool interactive = is_interactive();

    if (interactive) {
        std::ostringstream ss;
        ss << colors::kDim
           << "╭───────────────────────────────────────────────────────────────────────────────────"
              "───╮\n"
           << "│ " << colors::kReset << colors::kBold
           << "                🎟️   C I N E M A   B O X   O F F I C E   K I O S K  🎟️              "
              "    "
           << colors::kReset << colors::kDim << "│\n"
           << "│ " << colors::kReset << colors::kDim
           << "                    Box Office Counter • Command Selector                           "
              "  "
           << colors::kReset << colors::kDim << "│\n"
           << "├───────────────────────────────────────────────────────────────────────────────────"
              "───┤\n"
           << "│                                                                                   "
              "   │\n"
           << "│   " << colors::kReset << colors::kBrightGreen << "[1] LIST" << colors::kReset
           << "               View all 20 cinema seats & seating map                         "
           << colors::kDim << "│\n"
           << "│   " << colors::kReset << colors::kBrightCyan << "[2] RESERVE <seat_id>"
           << colors::kReset << "   Book a cinema seat  (e.g. 2 A1 or RESERVE A1)                 "
           << colors::kDim << "│\n"
           << "│   " << colors::kReset << colors::kBrightYellow << "[3] STATUS  <seat_id>"
           << colors::kReset << "   Inspect seat details (e.g. 3 A1 or STATUS A1)                 "
           << colors::kDim << "│\n"
           << "│   " << colors::kReset << colors::kBrightRed << "[4] CANCEL  <seat_id>"
           << colors::kReset << "   Cancel reservation  (e.g. 4 A1 or CANCEL A1)                 "
           << colors::kDim << "│\n"
           << "│   " << colors::kReset << colors::kWhite << "[5] HELP / H" << colors::kReset
           << "           Display this interactive command guide                        "
           << colors::kDim << "│\n"
           << "│   " << colors::kReset << colors::kMagenta << "[6] QUIT / EXIT" << colors::kReset
           << "        Leave Cinema Theater & close session                          "
           << colors::kDim << "│\n"
           << "│   " << colors::kReset << colors::kBrightCyan << "[7] CLEAR / CLS" << colors::kReset
           << "        Refresh & re-center Cinema Theater screen                     "
           << colors::kDim << "│\n"
           << "│                                                                                   "
              "   │\n"
           << "├───────────────────────────────────────────────────────────────────────────────────"
              "───┤\n"
           << "│ " << colors::kReset << colors::kDim
           << "  💡 Quick Shortcuts: Type \"H\" for menu, \"1\" for map, \"2 A1\" to book, \"7\" "
              "to clear! "
           << colors::kReset << colors::kDim << "│\n"
           << "╰───────────────────────────────────────────────────────────────────────────────────"
              "───╯\n"
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
        std::size_t pad = (width > 88) ? (static_cast<std::size_t>(width) - 88) / 2 : 0;
        std::string pad_str(pad, ' ');
        ss << pad_str << colors::kDim << "╭─ Ticket Booth #" << client_id
           << " @ Cinema Theater ────────────────────────────────────────────────╮\n"
           << pad_str << "╰─❯ " << colors::kReset << colors::kBold << colors::kBrightYellow;
    } else {
        ss << "Client[" << client_id << "]> ";
    }
    return ss.str();
}

std::string TerminalUi::format_success(std::string_view message, bool colorize) {
    std::ostringstream ss;
    if (colorize && is_interactive()) {
        ss << colors::kBold << colors::kBrightGreen << "  [SUCCESS] " << colors::kReset << message;
        return center_line(ss.str());
    }
    ss << "  [SUCCESS] " << message;
    return ss.str();
}

std::string TerminalUi::format_error(std::string_view message, bool colorize) {
    std::ostringstream ss;
    if (colorize && is_interactive()) {
        ss << colors::kBold << colors::kBrightRed << "  [FAILED] " << colors::kReset << message;
        return center_line(ss.str());
    }
    ss << "  [FAILED] " << message;
    return ss.str();
}

std::string TerminalUi::format_info(std::string_view message, bool colorize) {
    std::ostringstream ss;
    if (colorize && is_interactive()) {
        ss << colors::kBrightCyan << "  [INFO] " << colors::kReset << message;
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
            << colors::kDim
            << "╭──────────────────────────────────────────────────────────────────────────────────"
               "────╮\n"
            << "│" << colors::kReset << colors::kBold << colors::kBrightYellow
            << "                   ░▒▓██████  C I N E M A   S C R E E N  ██████▓▒░                 "
               "   "
            << colors::kReset << colors::kDim << "│\n"
            << "│" << colors::kReset << colors::kCyan
            << "                       ◄◄◄   DOLBY ATMOS • LASER IMAX   ►►►                        "
               "   "
            << colors::kReset << colors::kDim << "│\n"
            << "╰──────────────────────────────────────────────────────────────────────────────────"
               "────╯\n"
            << colors::kDim
            << "╭──────────────────────────────────────────────────────────────────────────────────"
               "────╮\n"
            << "│  " << colors::kReset << colors::kBold << "LEGEND:  " << colors::kReset
            << colors::kDim << "[" << colors::kReset << colors::kBrightGreen << " A1 • FREE "
            << colors::kDim << "] Available   "
            << "[" << colors::kReset << colors::kBold << colors::kBrightYellow << " A1 ★ (ME) "
            << colors::kDim << "] Your Seat   "
            << "[" << colors::kReset << colors::kBrightRed << " A1 🔒C#02 " << colors::kDim
            << "] Booked   " << "│\n"
            << "╰──────────────────────────────────────────────────────────────────────────────────"
               "────╯\n"
            << colors::kReset << "\n";
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
                out << "  " << colors::kBold << colors::kWhite << "Row " << current_row << ":"
                    << colors::kReset << "   ";
            } else {
                out << "  Row " << current_row << ":  ";
            }
        } else {
            out << (use_color ? "   " : " ");
        }

        std::ostringstream token;
        if (use_color) {
            if (seat.status == core::SeatStatus::Available) {
                token << colors::kDim << "[" << colors::kReset << colors::kBold << seat.seat_id
                      << colors::kDim << " • " << colors::kReset << colors::kBrightGreen << "FREE"
                      << colors::kDim << " ]" << colors::kReset;
            } else if (current_client_id != common::kInvalidClientId &&
                       seat.owner_client_id == current_client_id) {
                token << colors::kDim << "[" << colors::kReset << colors::kBold << seat.seat_id
                      << colors::kDim << " ★ " << colors::kReset << colors::kBold
                      << colors::kBrightYellow << "(ME)" << colors::kDim << " ]" << colors::kReset;
            } else {
                token << colors::kDim << "[" << colors::kReset << colors::kBold << seat.seat_id
                      << colors::kDim << " 🔒" << colors::kReset << colors::kBrightRed << "C#"
                      << std::setfill('0') << std::setw(2) << seat.owner_client_id << colors::kDim
                      << " ]" << colors::kReset;
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

    std::string_view bar_color = colors::kBrightGreen;
    if (occupancy_pct >= 85.0) {
        bar_color = colors::kBrightRed;
    } else if (occupancy_pct >= 60.0) {
        bar_color = colors::kBrightYellow;
    }

    if (use_color) {
        out << colors::kDim
            << "──────────────────────────────────────────────────────────────────────────────────"
               "────\n"
            << colors::kReset << "  Box Office: Total = " << total_seats << "  |  "
            << colors::kBrightGreen << "Available = " << available_count << colors::kReset
            << "  |  " << colors::kBrightYellow << "Reserved = " << reserved_count << colors::kReset
            << "  |  Dolby Status: " << colors::kBrightCyan << "ONLINE" << colors::kReset << "\n"
            << "  " << colors::kBold << "THEATER CAPACITY : " << colors::kReset << colors::kDim
            << "[" << colors::kReset << bar_color << filled_bar << colors::kReset << colors::kDim
            << empty_bar << colors::kReset << colors::kDim << "]  " << colors::kReset
            << colors::kBold << colors::kBrightYellow << std::fixed << std::setprecision(1)
            << occupancy_pct << "%" << colors::kReset << colors::kDim << "  (" << reserved_count
            << "/" << total_seats << " Booked)\n"
            << colors::kDim
            << "──────────────────────────────────────────────────────────────────────────────────"
               "────\n"
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

} // namespace css223::client
