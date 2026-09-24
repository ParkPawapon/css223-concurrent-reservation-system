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
    // Throws std::system_error if cleanup of the previous queue fails.
    PosixMessageQueue& operator=(PosixMessageQueue&& other);

    // The instance that creates a new queue owns its name and unlinks it on close.
    // Opening an existing queue or a read/write-only handle never takes ownership.
    static PosixMessageQueue open_or_create(std::string_view name,
                                            const QueueConfig& config = QueueConfig{});

    static PosixMessageQueue open_read_only(std::string_view name);
    static PosixMessageQueue open_write_only(std::string_view name);

    [[nodiscard]] bool is_open() const noexcept;
    // Returns false with errno set on failure. Failed cleanup can be retried,
    // even after the descriptor is closed; ownership is retained until unlink succeeds.
    bool close() noexcept;

    bool send(const void* data, std::size_t size, unsigned int priority = 0) const;
    bool receive(void* buffer, std::size_t size, unsigned int* priority = nullptr) const;

    // An already absent queue (ENOENT) also counts as successful cleanup.
    static bool unlink(std::string_view name) noexcept;

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

private:
    explicit PosixMessageQueue(mqd_t descriptor,
                               std::string name,
                               bool owns_queue = false) noexcept;

    mqd_t descriptor_{-1};
    std::string name_;
    bool owns_queue_{false};
};

} // namespace css223::ipc

#endif // RESERVATION_IPC_POSIX_MESSAGE_QUEUE_HPP
