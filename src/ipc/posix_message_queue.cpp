#include "ipc/posix_message_queue.hpp"

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#if defined(__linux__) || defined(__unix__)
    #include <fcntl.h>
    #include <mqueue.h>
    #include <sys/stat.h>
#endif

namespace css223::ipc {

namespace {

constexpr mqd_t kInvalidMqd = -1;

} // namespace

PosixMessageQueue::PosixMessageQueue(mqd_t descriptor, std::string name) noexcept
    : descriptor_(descriptor), name_(std::move(name)) {}

PosixMessageQueue::~PosixMessageQueue() {
    close();
}

PosixMessageQueue::PosixMessageQueue(PosixMessageQueue&& other) noexcept
    : descriptor_(other.descriptor_), name_(std::move(other.name_)) {
    other.descriptor_ = kInvalidMqd;
}

PosixMessageQueue& PosixMessageQueue::operator=(PosixMessageQueue&& other) noexcept {
    if (this != &other) {
        close();
        descriptor_ = other.descriptor_;
        name_ = std::move(other.name_);
        other.descriptor_ = kInvalidMqd;
    }
    return *this;
}

PosixMessageQueue PosixMessageQueue::open_or_create(std::string_view name,
                                                    const QueueConfig& config) {
    std::string name_str(name);

#if defined(__linux__) || defined(__unix__)
    struct mq_attr attr;
    std::memset(&attr, 0, sizeof(attr));
    attr.mq_flags = 0;
    attr.mq_maxmsg = config.max_messages;
    attr.mq_msgsize = config.max_message_size;
    attr.mq_curmsgs = 0;

    constexpr mode_t kQueuePermissions = 0660;
    mqd_t mqd = ::mq_open(name_str.c_str(), O_RDWR | O_CREAT, kQueuePermissions, &attr);

    if (mqd == kInvalidMqd) {
        return PosixMessageQueue(kInvalidMqd, name_str);
    }
    return PosixMessageQueue(mqd, name_str);
#else
    (void) config;
    return PosixMessageQueue(kInvalidMqd, name_str);
#endif
}

PosixMessageQueue PosixMessageQueue::open_read_only(std::string_view name) {
    std::string name_str(name);

#if defined(__linux__) || defined(__unix__)
    mqd_t mqd = ::mq_open(name_str.c_str(), O_RDONLY);
    return PosixMessageQueue(mqd, name_str);
#else
    return PosixMessageQueue(kInvalidMqd, name_str);
#endif
}

PosixMessageQueue PosixMessageQueue::open_write_only(std::string_view name) {
    std::string name_str(name);

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

void PosixMessageQueue::close() noexcept {
#if defined(__linux__) || defined(__unix__)
    if (descriptor_ != kInvalidMqd) {
        ::mq_close(descriptor_);
        descriptor_ = kInvalidMqd;
    }
#else
    descriptor_ = kInvalidMqd;
#endif
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

    return bytes_read >= 0;
#else
    (void) size;
    (void) priority;
    return false;
#endif
}

bool PosixMessageQueue::unlink(std::string_view name) noexcept {
#if defined(__linux__) || defined(__unix__)
    std::string name_str(name);
    return ::mq_unlink(name_str.c_str()) == 0;
#else
    (void) name;
    return false;
#endif
}

} // namespace css223::ipc
