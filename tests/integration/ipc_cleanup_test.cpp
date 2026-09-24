#include <cerrno>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unistd.h>
#include <utility>

#include "ipc/posix_message_queue.hpp"

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

int injected_error = 0;
unsigned int failures_remaining = 0;

std::string queue_name(const char* suffix) {
    return "/css223_cleanup_" + std::to_string(::getpid()) + "_" + suffix;
}

void fail_next_unlink(int error) {
    injected_error = error;
    failures_remaining = 1;
}

bool queue_exists(const std::string& name) {
    return css223::ipc::PosixMessageQueue::open_read_only(name).is_open();
}

void test_close_retry() {
    const auto name = queue_name("retry");
    auto queue = css223::ipc::PosixMessageQueue::open_or_create(name);
    require(queue.is_open(), "create queue for cleanup retry");
    fail_next_unlink(EACCES);
    require(!queue.close() && errno == EACCES, "close reports unlink failure");
    require(!queue.is_open() && queue_exists(name), "descriptor closed but name still exists");
    require(queue.close(), "close retries pending unlink");
    require(!queue_exists(name), "retry removed the queue name");
    require(queue.close(), "close is idempotent");
}

void test_destructor_retry() {
    const auto name = queue_name("destructor");
    {
        auto queue = css223::ipc::PosixMessageQueue::open_or_create(name);
        require(queue.is_open(), "create queue for destructor retry");
        fail_next_unlink(EACCES);
        require(!queue.close(), "first cleanup attempt fails");
    }
    require(!queue_exists(name), "destructor retries the failed unlink");
}

void test_interrupted_and_repeated_unlink() {
    const auto name = queue_name("interrupted");
    auto queue = css223::ipc::PosixMessageQueue::open_or_create(name);
    require(queue.is_open(), "create queue for interrupted unlink");
    fail_next_unlink(EINTR);
    require(queue.close(), "unlink retries EINTR");
    require(!queue_exists(name), "interrupted unlink eventually removed queue");
    require(css223::ipc::PosixMessageQueue::unlink(name), "ENOENT counts as cleanup success");

    queue = css223::ipc::PosixMessageQueue::open_or_create(name);
    require(queue.is_open(), "recreate queue");
    require(css223::ipc::PosixMessageQueue::unlink(name), "explicit unlink");
    require(queue.close(), "owner close succeeds after explicit unlink");
}

void test_move_does_not_lose_pending_cleanup() {
    const auto source_name = queue_name("source");
    const auto destination_name = queue_name("destination");
    auto source = css223::ipc::PosixMessageQueue::open_or_create(source_name);
    auto destination = css223::ipc::PosixMessageQueue::open_or_create(destination_name);
    require(source.is_open() && destination.is_open(), "create queues for move assignment");
    fail_next_unlink(EACCES);
    bool rejected = false;
    try {
        destination = std::move(source);
    } catch (const std::system_error& error) {
        rejected = error.code().value() == EACCES;
    }
    require(rejected, "move reports failed cleanup instead of discarding ownership");
    require(source.is_open() && destination.name() == destination_name,
            "failed move preserves source and destination cleanup state");
    require(destination.close(), "retry destination cleanup");
    require(!queue_exists(destination_name), "old destination removed");
    destination = std::move(source);
    require(destination.is_open() && !source.is_open(), "move succeeds after cleanup");
    require(destination.close() && !queue_exists(source_name), "moved ownership cleans up");
}

} // namespace

extern "C" int __real_mq_unlink(const char* name);
extern "C" int __wrap_mq_unlink(const char* name);

extern "C" int __wrap_mq_unlink(const char* name) {
    if (failures_remaining != 0) {
        --failures_remaining;
        errno = injected_error;
        return -1;
    }
    return __real_mq_unlink(name);
}

int main() {
    try {
        test_close_retry();
        test_destructor_retry();
        test_interrupted_and_repeated_unlink();
        test_move_does_not_lose_pending_cleanup();
    } catch (const std::exception& error) {
        std::cerr << "IPC cleanup test failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
