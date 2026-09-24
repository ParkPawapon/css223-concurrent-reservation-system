#ifndef RESERVATION_CLIENT_CLIENT_REPL_HPP
#define RESERVATION_CLIENT_CLIENT_REPL_HPP

#include <iosfwd>
#include <string_view>

#include "client/client.hpp"

namespace css223::client {

class ClientRepl {
public:
    explicit ClientRepl(Client& client) noexcept;

    void run(std::istream& in, std::ostream& out);

    void handle_list(std::ostream& out);
    void handle_status(std::string_view seat_id, std::ostream& out);
    void handle_reserve(std::string_view seat_id, std::ostream& out);
    void handle_cancel(std::string_view seat_id, std::ostream& out);
    void handle_quit(std::ostream& out);
    static void handle_help(std::ostream& out);

private:
    Client& client_;
};

} // namespace css223::client

#endif // RESERVATION_CLIENT_CLIENT_REPL_HPP
