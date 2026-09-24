#ifndef RESERVATION_CLIENT_CLIENT_HPP
#define RESERVATION_CLIENT_CLIENT_HPP

#include <optional>
#include <string>
#include <string_view>

#include "common/command.hpp"
#include "common/types.hpp"
#include "ipc/message.hpp"
#include "ipc/posix_message_queue.hpp"

namespace css223::client {

class Client {
public:
    explicit Client(common::ClientId client_id);
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    Client(Client&&) = default;
    Client& operator=(Client&&) = default;

    bool connect(std::string_view server_queue_name = {});
    void disconnect() noexcept;

    [[nodiscard]] bool is_connected() const noexcept;
    [[nodiscard]] common::ClientId client_id() const noexcept { return client_id_; }
    [[nodiscard]] const std::string& reply_queue_name() const noexcept { return reply_queue_name_; }

    bool send_request(common::CommandType command, std::string_view seat_id = {});
    [[nodiscard]] std::optional<ipc::ResponseMessage> receive_response();

private:
    common::ClientId client_id_;
    common::RequestId next_request_id_{1};
    std::string reply_queue_name_;
    ipc::PosixMessageQueue reply_queue_;
    ipc::PosixMessageQueue server_queue_;
};

} // namespace css223::client

#endif // RESERVATION_CLIENT_CLIENT_HPP
