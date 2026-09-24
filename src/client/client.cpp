#include "client/client.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "common/constants.hpp"
#include "ipc/message.hpp"
#include "ipc/queue_names.hpp"

namespace css223::client {

Client::Client(common::ClientId client_id)
    : client_id_(client_id), reply_queue_name_(ipc::format_client_reply_queue_name(client_id)) {}

Client::~Client() {
    disconnect();
}

bool Client::connect(std::string_view server_queue_name) {
    disconnect();

    std::string_view target_server_queue =
        server_queue_name.empty() ? ipc::get_server_request_queue_name() : server_queue_name;

    // Client explicitly owns and creates its reply queue lifecycle
    ipc::PosixMessageQueue::unlink(reply_queue_name_);

    ipc::QueueConfig reply_config{};
    reply_config.max_messages = 10;
    reply_config.max_message_size = sizeof(ipc::ResponseMessage);

    reply_queue_ = ipc::PosixMessageQueue::open_or_create(reply_queue_name_, reply_config);
    if (!reply_queue_.is_open()) {
        return false;
    }

    server_queue_ = ipc::PosixMessageQueue::open_write_only(target_server_queue);
    if (!server_queue_.is_open()) {
        reply_queue_.close();
        ipc::PosixMessageQueue::unlink(reply_queue_name_);
        return false;
    }

    return true;
}

void Client::disconnect() noexcept {
    server_queue_.close();
    reply_queue_.close();
    ipc::PosixMessageQueue::unlink(reply_queue_name_);
}

bool Client::is_connected() const noexcept {
    return reply_queue_.is_open() && server_queue_.is_open();
}

bool Client::send_request(common::CommandType command, std::string_view seat_id) {
    if (!server_queue_.is_open()) {
        return false;
    }

    ipc::RequestMessage request{};
    request.request_id = next_request_id_++;
    request.client_id = client_id_;
    request.command = command;
    ipc::copy_string_to_buffer(request.seat_id, sizeof(request.seat_id), seat_id);
    ipc::copy_string_to_buffer(
        request.reply_queue_name, sizeof(request.reply_queue_name), reply_queue_name_);

    return server_queue_.send(&request, sizeof(request));
}

std::optional<ipc::ResponseMessage> Client::receive_response() {
    if (!reply_queue_.is_open()) {
        return std::nullopt;
    }

    ipc::ResponseMessage response{};
    if (!reply_queue_.receive(&response, sizeof(response))) {
        return std::nullopt;
    }

    return response;
}

} // namespace css223::client
