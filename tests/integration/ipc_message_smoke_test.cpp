#include <cassert>
#include <cstdlib>
#include <string_view>
#include <type_traits>

#include "common/constants.hpp"
#include "ipc/message.hpp"
#include "ipc/queue_names.hpp"

int main() {
    // 1. Verify wire-safety constraints for POSIX MQ
    static_assert(std::is_trivially_copyable_v<css223::ipc::RequestMessage>);
    static_assert(std::is_trivially_copyable_v<css223::ipc::ResponseMessage>);

    // 2. Verify buffer copying helpers
    char buffer[16]{0};
    css223::ipc::copy_string_to_buffer(buffer, sizeof(buffer), "TEST_SEAT");
    const std::string_view sv = css223::ipc::buffer_to_string_view(buffer, sizeof(buffer));
    assert(sv == "TEST_SEAT");

    // 3. Verify queue naming
    assert(css223::ipc::get_server_request_queue_name() == css223::common::kDefaultServerQueueName);
    const std::string client_queue = css223::ipc::format_client_reply_queue_name(5);
    assert(client_queue == "/css223_client_5_reply");

    // 4. Verify POSIX queue name constraints
    assert(css223::ipc::is_valid_posix_queue_name("/valid_queue"));
    assert(css223::ipc::is_valid_posix_queue_name(css223::common::kDefaultServerQueueName));
    assert(!css223::ipc::is_valid_posix_queue_name("invalid_no_slash"));
    assert(!css223::ipc::is_valid_posix_queue_name("/nested/slash"));

    return EXIT_SUCCESS;
}
