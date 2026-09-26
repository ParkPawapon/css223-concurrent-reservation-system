#include "ipc/posix_message_queue.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#include "common/constants.hpp"
#include "ipc/queue_names.hpp"

#if defined(__linux__) || defined(__unix__)
    #include <fcntl.h>
    #include <mqueue.h>
    #include <sys/stat.h>
#endif

namespace css223::ipc {

namespace {

constexpr mqd_t kInvalidMqd = -1;

} // namespace

PosixMessageQueue::PosixMessageQueue(mqd_t descriptor, std::string name, bool owns_queue) noexcept
    : descriptor_(descriptor), name_(std::move(name)), owns_queue_(owns_queue) {}

PosixMessageQueue::~PosixMessageQueue() {
    if (!close()) {
        const int kError = errno;
        std::fprintf(
            stderr, "[IPC] Failed to close/unlink queue %s (errno=%d)\n", name_.c_str(), kError);
    }
}

PosixMessageQueue::PosixMessageQueue(PosixMessageQueue&& other) noexcept
    : descriptor_(other.descriptor_), name_(std::move(other.name_)),
      owns_queue_(other.owns_queue_) {
    other.descriptor_ = kInvalidMqd;
    other.owns_queue_ = false;
}

PosixMessageQueue& PosixMessageQueue::operator=(PosixMessageQueue&& other) noexcept {
    if (this != &other) {
        if (!close()) {
            const int kError = errno;
            std::fprintf(stderr,
                         "[IPC] Failed to close/unlink queue %s before move assignment "
                         "(errno=%d)\n",
                         name_.c_str(),
                         kError);
        }
        descriptor_ = other.descriptor_;
        name_ = std::move(other.name_);
        owns_queue_ = other.owns_queue_;
        other.descriptor_ = kInvalidMqd;
        other.owns_queue_ = false;
    }
    return *this;
}

PosixMessageQueue PosixMessageQueue::open_or_create(std::string_view name,
                                                    const QueueConfig& config) {
    std::string name_str(name);

    if (!is_valid_posix_queue_name(name) || config.max_messages <= 0 ||
        config.max_message_size <= 0) {
        return PosixMessageQueue(kInvalidMqd, std::move(name_str));
    }

#if defined(__linux__) || defined(__unix__)
    struct mq_attr attr {};
    attr.mq_maxmsg = config.max_messages;
    attr.mq_msgsize = config.max_message_size;
    constexpr mode_t kQueuePermissions = 0660;
    mqd_t mqd = ::mq_open(name_str.c_str(), O_RDWR | O_CREAT | O_EXCL, kQueuePermissions, &attr);
    bool owns_queue = mqd != kInvalidMqd;

    if (mqd == kInvalidMqd && errno == EEXIST) {
        mqd = ::mq_open(name_str.c_str(), O_RDWR);
        if (mqd != kInvalidMqd) {
            struct mq_attr existing_attr {};
            if (::mq_getattr(mqd, &existing_attr) == -1 ||
                existing_attr.mq_msgsize != config.max_message_size ||
                existing_attr.mq_maxmsg != config.max_messages) {
                ::mq_close(mqd);
                mqd = kInvalidMqd;
            }
        }
    }

    if (mqd == kInvalidMqd) {
        return PosixMessageQueue(kInvalidMqd, std::move(name_str));
    }
    return PosixMessageQueue(mqd, std::move(name_str), owns_queue);
#else
    (void) config;
    return PosixMessageQueue(kInvalidMqd, std::move(name_str));
#endif
}

PosixMessageQueue PosixMessageQueue::open_read_only(std::string_view name) {
    std::string name_str(name);
    if (!is_valid_posix_queue_name(name)) {
        return PosixMessageQueue(kInvalidMqd, std::move(name_str));
    }

#if defined(__linux__) || defined(__unix__)
    mqd_t mqd = ::mq_open(name_str.c_str(), O_RDONLY);
    return PosixMessageQueue(mqd, name_str);
#else
    return PosixMessageQueue(kInvalidMqd, name_str);
#endif
}

PosixMessageQueue PosixMessageQueue::open_write_only(std::string_view name) {
    std::string name_str(name);
    if (!is_valid_posix_queue_name(name)) {
        return PosixMessageQueue(kInvalidMqd, std::move(name_str));
    }

#if defined(__linux__) || defined(__unix__)
    mqd_t mqd = ::mq_open(name_str.c_str(), O_WRONLY);
    return PosixMessageQueue(mqd, name_str);
#else
    return PosixMessageQueue(kInvalidMqd, name_str);
#endif
}

bool PosixMessageQueue::is_open() const noexcept {
    return descriptor_ != kInvalidMqd;
}

bool PosixMessageQueue::close() noexcept {
    int error = 0;
#if defined(__linux__) || defined(__unix__)
    if (descriptor_ != kInvalidMqd) {
        if (::mq_close(descriptor_) == 0) {
            descriptor_ = kInvalidMqd;
        } else {
            error = errno;
        }
    }
    if (owns_queue_) {
        if (unlink(name_)) {
            owns_queue_ = false;
        } else if (error == 0) {
            error = errno;
        }
    }
#else
    descriptor_ = kInvalidMqd;
    owns_queue_ = false;
#endif
    if (error != 0) {
        errno = error;
        return false;
    }
    return true;
}

bool PosixMessageQueue::send(const void* data, std::size_t size, unsigned int priority) const {
    if (!is_open() || data == nullptr) {
        return false;
    }

#if defined(__linux__) || defined(__unix__)
    int result = 0;
    do {
        result = ::mq_send(descriptor_, static_cast<const char*>(data), size, priority);
    } while (result == -1 && errno == EINTR);

    return result == 0;
#else
    (void) size;
    (void) priority;
    return false;
#endif
}

bool PosixMessageQueue::receive(void* buffer, std::size_t size, unsigned int* priority) const {
    if (!is_open() || buffer == nullptr) {
        return false;
    }

#if defined(__linux__) || defined(__unix__)
    ssize_t bytes_read = 0;
    do {
        bytes_read = ::mq_receive(descriptor_, static_cast<char*>(buffer), size, priority);
    } while (bytes_read == -1 && errno == EINTR);

    return bytes_read >= 0 && static_cast<std::size_t>(bytes_read) == size;
#else
    (void) size;
    (void) priority;
    return false;
#endif
}

bool PosixMessageQueue::unlink(std::string_view name) noexcept {
#if defined(__linux__) || defined(__unix__)
    if (!is_valid_posix_queue_name(name)) {
        errno = EINVAL;
        return false;
    }
    std::array<char, common::kMaxQueueNameLength> name_buffer{};
    std::memcpy(name_buffer.data(), name.data(), name.size());
    int result = 0;
    do {
        result = ::mq_unlink(name_buffer.data());
    } while (result == -1 && errno == EINTR);
    return result == 0 || errno == ENOENT;
#else
    (void) name;
    return false;
#endif
}

} // namespace css223::ipc
