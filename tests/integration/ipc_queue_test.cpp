#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

#include "common/command.hpp"
#include "common/result.hpp"
#include "core/seat.hpp"
#include "ipc/message.hpp"
#include "ipc/posix_message_queue.hpp"
#include "ipc/queue_names.hpp"

namespace {

using css223::ipc::PosixMessageQueue;
using css223::ipc::QueueConfig;
using css223::ipc::RequestMessage;
using css223::ipc::ResponseMessage;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string test_queue_name(const char* suffix) {
    return "/css223_ipc_test_" + std::to_string(::getpid()) + "_" + suffix;
}

void test_request_response_between_processes() {
    const std::string request_name = test_queue_name("request");
    const std::string reply_name = test_queue_name("reply");

    QueueConfig request_config{};
    request_config.max_message_size = sizeof(RequestMessage);
    QueueConfig reply_config{};
    reply_config.max_message_size = sizeof(ResponseMessage);

    auto request_queue = PosixMessageQueue::open_or_create(request_name, request_config);
    auto reply_queue = PosixMessageQueue::open_or_create(reply_name, reply_config);
    require(request_queue.is_open() && reply_queue.is_open(), "create test queues");

    const pid_t child = ::fork();
    require(child >= 0, "fork child process");

    if (child == 0) {
        try {
            auto request_reader = PosixMessageQueue::open_read_only(request_name);
            auto reply_writer = PosixMessageQueue::open_write_only(reply_name);
            require(request_reader.is_open() && reply_writer.is_open(), "open queues in child");

            RequestMessage received{};
            require(request_reader.receive(&received, sizeof(received)), "child receives request");
            require(received.request_id == 73 && received.client_id == 5,
                    "request identifiers survive transfer");
            require(received.command == css223::common::CommandType::Reserve,
                    "request command survives transfer");
            require(css223::ipc::buffer_to_string_view(received.seat_id,
                                                       sizeof(received.seat_id)) == "A1",
                    "request seat survives transfer");
            require(css223::ipc::buffer_to_string_view(
                        received.reply_queue_name, sizeof(received.reply_queue_name)) == reply_name,
                    "reply queue name survives transfer");

            ResponseMessage response{};
            response.request_id = received.request_id;
            response.client_id = received.client_id;
            response.result = css223::common::StatusCode::Success;
            response.status = css223::core::SeatStatus::Reserved;
            response.owner_client_id = received.client_id;
            css223::ipc::copy_string_to_buffer(response.seat_id, sizeof(response.seat_id), "A1");
            css223::ipc::copy_string_to_buffer(
                response.message, sizeof(response.message), "reserved");
            require(reply_writer.send(&response, sizeof(response)), "child sends response");
            _exit(EXIT_SUCCESS);
        } catch (const std::exception&) {
            _exit(EXIT_FAILURE);
        }
    }

    RequestMessage request{};
    request.request_id = 73;
    request.client_id = 5;
    request.command = css223::common::CommandType::Reserve;
    css223::ipc::copy_string_to_buffer(request.seat_id, sizeof(request.seat_id), "A1");
    css223::ipc::copy_string_to_buffer(
        request.reply_queue_name, sizeof(request.reply_queue_name), reply_name);
    require(request_queue.send(&request, sizeof(request)), "parent sends request");

    ResponseMessage response{};
    require(reply_queue.receive(&response, sizeof(response)), "parent receives response");
    require(response.request_id == 73 && response.client_id == 5,
            "response identifiers survive transfer");
    require(response.result == css223::common::StatusCode::Success &&
                response.status == css223::core::SeatStatus::Reserved &&
                response.owner_client_id == 5,
            "response state survives transfer");
    require(css223::ipc::buffer_to_string_view(response.seat_id, sizeof(response.seat_id)) == "A1",
            "response seat survives transfer");
    require(css223::ipc::buffer_to_string_view(response.message, sizeof(response.message)) ==
                "reserved",
            "response text survives transfer");

    int status = 0;
    require(::waitpid(child, &status, 0) == child && WIFEXITED(status) &&
                WEXITSTATUS(status) == EXIT_SUCCESS,
            "child completed successfully");

    request_queue.close();
    reply_queue.close();
    require(!PosixMessageQueue::open_read_only(request_name).is_open(),
            "request queue unlinked after close");
    require(!PosixMessageQueue::open_read_only(reply_name).is_open(),
            "reply queue unlinked after close");
}

void test_ownership_and_message_sizes() {
    const std::string name = test_queue_name("ownership");
    QueueConfig config{};
    config.max_message_size = sizeof(RequestMessage);

    auto owner = PosixMessageQueue::open_or_create(name, config);
    require(owner.is_open(), "create owned queue");
    auto shared = PosixMessageQueue::open_or_create(name, config);
    require(shared.is_open(), "open existing queue");
    shared.close();
    require(PosixMessageQueue::open_read_only(name).is_open(), "closing non-owner preserves queue");

    auto moved_owner = std::move(owner);
    owner.close();
    require(PosixMessageQueue::open_read_only(name).is_open(), "moved-from queue does not unlink");

    auto writer = PosixMessageQueue::open_write_only(name);
    require(writer.is_open(), "open writer");
    RequestMessage request{};
    request.request_id = 91;
    require(writer.send(&request, sizeof(request)), "send correctly sized message");
    char short_buffer[sizeof(RequestMessage) - 1]{};
    require(!moved_owner.receive(short_buffer, sizeof(short_buffer)),
            "reject receive buffer smaller than queue message size");
    RequestMessage received{};
    require(moved_owner.receive(&received, sizeof(received)) && received.request_id == 91,
            "failed receive does not consume queued message");

    const char short_message = 'x';
    require(writer.send(&short_message, sizeof(short_message)), "send short raw message");
    require(!moved_owner.receive(&received, sizeof(received)),
            "reject message shorter than expected struct");

    writer.close();
    moved_owner.close();
    require(!PosixMessageQueue::open_write_only(name).is_open(),
            "owned queue unlinked after close");
}

void test_destructor_and_stale_queue_cleanup() {
    const std::string owned_name = test_queue_name("destructor");
    {
        auto owner = PosixMessageQueue::open_or_create(owned_name);
        require(owner.is_open(), "create queue for destructor cleanup");
    }
    require(!PosixMessageQueue::open_read_only(owned_name).is_open(),
            "destructor unlinks owned queue");

    const std::string stale_name = test_queue_name("stale");
    const pid_t child = ::fork();
    require(child >= 0, "fork for stale queue test");
    if (child == 0) {
        auto owner = PosixMessageQueue::open_or_create(stale_name);
        _exit(owner.is_open() ? EXIT_SUCCESS : EXIT_FAILURE);
    }
    int status = 0;
    require(::waitpid(child, &status, 0) == child && WIFEXITED(status) &&
                WEXITSTATUS(status) == EXIT_SUCCESS,
            "child created queue before abrupt exit");
    require(PosixMessageQueue::open_read_only(stale_name).is_open(),
            "queue persists after abrupt process exit");
    require(PosixMessageQueue::unlink(stale_name), "remove stale queue");
    require(!PosixMessageQueue::open_read_only(stale_name).is_open(),
            "stale queue absent after unlink");
}

void test_embedded_null_cannot_unlink_another_queue() {
    const std::string name = test_queue_name("nul_victim");
    auto owner = PosixMessageQueue::open_or_create(name);
    require(owner.is_open(), "create queue protected from embedded NUL aliasing");

    std::string malicious_name = name;
    malicious_name.push_back('\0');
    malicious_name += "suffix";
    require(
        !PosixMessageQueue::unlink(std::string_view(malicious_name.data(), malicious_name.size())),
        "reject queue name containing embedded NUL");
    require(PosixMessageQueue::open_read_only(name).is_open(),
            "embedded NUL unlink attempt preserves original queue");
    require(owner.close(), "clean up protected queue");
}

} // namespace

int main() {
    try {
        require(css223::ipc::get_server_request_queue_name() == "/css223_reservation_requests",
                "server queue name");
        require(css223::ipc::format_client_reply_queue_name(5) == "/css223_client_5_reply",
                "client queue name");
        require(!PosixMessageQueue::open_or_create("invalid").is_open(),
                "invalid queue name rejected");
        test_request_response_between_processes();
        test_ownership_and_message_sizes();
        test_destructor_and_stale_queue_cleanup();
        test_embedded_null_cannot_unlink_another_queue();
    } catch (const std::exception& error) {
        std::cerr << "IPC queue integration test failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
