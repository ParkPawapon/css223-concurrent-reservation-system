#ifndef RESERVATION_IPC_POSIX_MESSAGE_QUEUE_HPP
#define RESERVATION_IPC_POSIX_MESSAGE_QUEUE_HPP

#include <cstddef>
#include <string>
#include <string_view>

#if defined(__linux__) || defined(__unix__)
    #include <fcntl.h>
    #include <mqueue.h>
    #include <sys/stat.h>
#else
using mqd_t = int;
#endif

namespace css223::ipc {

struct QueueConfig {
    long max_messages{10};
    long max_message_size{256};
};

class PosixMessageQueue {
public:
    PosixMessageQueue() noexcept = default;
    ~PosixMessageQueue();

    PosixMessageQueue(const PosixMessageQueue&) = delete;
    PosixMessageQueue& operator=(const PosixMessageQueue&) = delete;

    PosixMessageQueue(PosixMessageQueue&& other) noexcept;
    PosixMessageQueue& operator=(PosixMessageQueue&& other) noexcept;

    static PosixMessageQueue open_or_create(std::string_view name,
                                            const QueueConfig& config = QueueConfig{});

    static PosixMessageQueue open_read_only(std::string_view name);
    static PosixMessageQueue open_write_only(std::string_view name);

    [[nodiscard]] bool is_open() const noexcept;
    void close() noexcept;

    bool send(const void* data, std::size_t size, unsigned int priority = 0) const;
    bool receive(void* buffer, std::size_t size, unsigned int* priority = nullptr) const;

    static bool unlink(std::string_view name) noexcept;

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

private:
    explicit PosixMessageQueue(mqd_t descriptor, std::string name) noexcept;

    mqd_t descriptor_{-1};
    std::string name_;
};

} // namespace css223::ipc

#endif // RESERVATION_IPC_POSIX_MESSAGE_QUEUE_HPP
