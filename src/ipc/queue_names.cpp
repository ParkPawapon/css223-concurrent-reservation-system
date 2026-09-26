#include "ipc/queue_names.hpp"

#include <string>
#include <string_view>

#include "common/constants.hpp"
#include "common/types.hpp"

namespace css223::ipc {

std::string format_client_reply_queue_name(common::ClientId client_id) {
    return std::string(common::kClientQueuePrefix) + std::to_string(client_id) + "_reply";
}

bool is_valid_posix_queue_name(std::string_view queue_name) noexcept {
    if (queue_name.empty() || queue_name.front() != '/' || queue_name.size() < 2) {
        return false;
    }
    if (queue_name.size() >= common::kMaxQueueNameLength) {
        return false;
    }
    // POSIX APIs consume a C string. Reject embedded NUL bytes so validation
    // and the kernel always operate on the same queue name.
    if (queue_name.find('\0') != std::string_view::npos) {
        return false;
    }
    // POSIX message queue names cannot contain subsequent slashes.
    if (queue_name.find('/', 1) != std::string_view::npos) {
        return false;
    }
    return true;
}

} // namespace css223::ipc
