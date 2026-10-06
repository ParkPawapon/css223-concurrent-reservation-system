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

// INTECH-inspired true-color palette. These values intentionally live beside the
// ANSI fallbacks above so the terminal UI remains target-based, deterministic,
// and independent from the host terminal theme.
inline constexpr std::string_view kBrandBlue = "\033[38;2;49;67;255m";
inline constexpr std::string_view kBrandBlueSoft = "\033[38;2;128;142;255m";
inline constexpr std::string_view kBrandWhite = "\033[38;2;246;247;255m";
inline constexpr std::string_view kBrandMuted = "\033[38;2;139;146;171m";
inline constexpr std::string_view kBrandLine = "\033[38;2;60;68;101m";
inline constexpr std::string_view kBrandSuccess = "\033[38;2;65;225;171m";
inline constexpr std::string_view kBrandWarning = "\033[38;2;255;211;104m";
inline constexpr std::string_view kBrandDanger = "\033[38;2;255;101;132m";

} // namespace colors

class TerminalUi {
public:
    static constexpr std::size_t kStandardBlockWidth = 96;

    [[nodiscard]] static bool is_interactive() noexcept;
    [[nodiscard]] static int get_terminal_width() noexcept;
    [[nodiscard]] static std::size_t visual_width(std::string_view line) noexcept;
    [[nodiscard]] static std::string get_padding(int width = 0) noexcept;
    [[nodiscard]] static std::string center_line(std::string_view line, int width = 0);
    [[nodiscard]] static std::string center_block(std::string_view block, int width = 0);

    static void init_signal_handlers() noexcept;
    [[nodiscard]] static bool has_resized() noexcept;
    static void reset_resized() noexcept;

    [[nodiscard]] static std::string sanitize_input(std::string_view input);

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

    static void render_kiosk_view(std::ostream& out,
                                  const std::vector<SeatDisplayInfo>& seats,
                                  common::ClientId client_id,
                                  std::string_view feedback_msg = "",
                                  std::string_view feedback_type = "",
                                  std::string_view ticket_seat_id = "");
};

} // namespace css223::client

#endif // RESERVATION_CLIENT_TERMINAL_UI_HPP
