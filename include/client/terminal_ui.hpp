#ifndef RESERVATION_CLIENT_TERMINAL_UI_HPP
#define RESERVATION_CLIENT_TERMINAL_UI_HPP

#include <cstddef>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "client/seat_map_formatter.hpp"
#include "common/constants.hpp"
#include "common/types.hpp"

namespace css223::client {

namespace colors {

inline constexpr std::string_view kReset = "\033[0m";
inline constexpr std::string_view kBold = "\033[1m";
inline constexpr std::string_view kDim = "\033[2m";
inline constexpr std::string_view kItalic = "\033[3m";
inline constexpr std::string_view kGreen = "\033[32m";
inline constexpr std::string_view kBrightGreen = "\033[92m";
inline constexpr std::string_view kRed = "\033[31m";
inline constexpr std::string_view kBrightRed = "\033[91m";
inline constexpr std::string_view kYellow = "\033[33m";
inline constexpr std::string_view kBrightYellow = "\033[93m";
inline constexpr std::string_view kCyan = "\033[36m";
inline constexpr std::string_view kBrightCyan = "\033[96m";
inline constexpr std::string_view kMagenta = "\033[35m";
inline constexpr std::string_view kBrightMagenta = "\033[95m";
inline constexpr std::string_view kWhite = "\033[37m";
inline constexpr std::string_view kBrightWhite = "\033[97m";

} // namespace colors

class TerminalUi {
public:
    static constexpr std::size_t kCinemaContentWidth = 88;

    [[nodiscard]] static bool is_interactive() noexcept;
    [[nodiscard]] static int get_terminal_width() noexcept;
    [[nodiscard]] static std::size_t visual_width(std::string_view line) noexcept;
    [[nodiscard]] static std::string center_line(std::string_view line, int width = 0);
    [[nodiscard]] static std::string center_block(std::string_view block, int width = 0);

    static void clear_screen(std::ostream& out);

    static void
    show_welcome_banner(std::ostream& out, common::ClientId client_id, bool animate = true);

    static void show_help_box(std::ostream& out);

    static void
    animate_ticket_print(std::ostream& out, std::string_view seat_id, common::ClientId client_id);

    static void
    print_ticket_stub(std::ostream& out, std::string_view seat_id, common::ClientId client_id);

    [[nodiscard]] static std::string format_prompt(common::ClientId client_id,
                                                   bool colorize = true);

    [[nodiscard]] static std::string format_success(std::string_view message, bool colorize = true);

    [[nodiscard]] static std::string format_error(std::string_view message, bool colorize = true);

    [[nodiscard]] static std::string format_info(std::string_view message, bool colorize = true);

    [[nodiscard]] static std::string
    format_grid(const std::vector<SeatDisplayInfo>& seats,
                bool colorize = true,
                common::ClientId current_client_id = common::kInvalidClientId);
};

} // namespace css223::client

#endif // RESERVATION_CLIENT_TERMINAL_UI_HPP
