#include "ipc/message.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string_view>

namespace css223::ipc {

void copy_string_to_buffer(char* dest, std::size_t dest_size, std::string_view src) noexcept {
    if (dest == nullptr || dest_size == 0) {
        return;
    }

    std::size_t copy_length = std::min(src.size(), dest_size - 1);
    if (copy_length > 0) {
        std::memcpy(dest, src.data(), copy_length);
    }
    dest[copy_length] = '\0';
}

std::string_view buffer_to_string_view(const char* src, std::size_t max_size) noexcept {
    if (src == nullptr || max_size == 0) {
        return {};
    }

    std::size_t len = strnlen(src, max_size);
    return {src, len};
}

} // namespace css223::ipc
