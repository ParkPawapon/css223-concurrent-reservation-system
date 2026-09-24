#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <pthread.h>
#include <signal.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>

#include "ipc/message.hpp"
#include "ipc/posix_message_queue.hpp"

namespace {

volatile sig_atomic_t signal_count = 0;

extern "C" void handle_signal(int /*signal*/) {
    ++signal_count;
}

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string test_queue_name(const char* suffix) {
    return "/css223_ipc_eintr_" + std::to_string(::getpid()) + "_" + suffix;
}

void wait_for_worker(const std::atomic<bool>& entered) {
    while (!entered.load()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void test_interrupted_receive() {
    css223::ipc::QueueConfig config{};
    config.max_message_size = sizeof(css223::ipc::RequestMessage);
    auto queue = css223::ipc::PosixMessageQueue::open_or_create(test_queue_name("receive"), config);
    require(queue.is_open(), "create receive test queue");

    std::atomic<bool> entered{false};
    bool received = false;
    css223::ipc::RequestMessage message{};
    std::thread worker([&]() {
        entered.store(true);
        received = queue.receive(&message, sizeof(message));
    });
    wait_for_worker(entered);
    const int signal_result = ::pthread_kill(worker.native_handle(), SIGUSR1);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    css223::ipc::RequestMessage request{};
    request.request_id = 42;
    const bool sent = queue.send(&request, sizeof(request));
    worker.join();
    require(signal_result == 0 && signal_count > 0, "signal reached waiting receiver");
    require(sent && received && message.request_id == 42,
            "receive retries after interrupted system call");
}

void test_interrupted_send() {
    css223::ipc::QueueConfig config{};
    config.max_messages = 1;
    config.max_message_size = sizeof(css223::ipc::RequestMessage);
    auto queue = css223::ipc::PosixMessageQueue::open_or_create(test_queue_name("send"), config);
    require(queue.is_open(), "create send test queue");

    css223::ipc::RequestMessage first{};
    first.request_id = 1;
    require(queue.send(&first, sizeof(first)), "fill queue before blocked send");

    std::atomic<bool> entered{false};
    bool sent = false;
    css223::ipc::RequestMessage second{};
    second.request_id = 2;
    std::thread worker([&]() {
        entered.store(true);
        sent = queue.send(&second, sizeof(second));
    });
    wait_for_worker(entered);
    const sig_atomic_t previous_signal_count = signal_count;
    const int signal_result = ::pthread_kill(worker.native_handle(), SIGUSR1);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    css223::ipc::RequestMessage received_first{};
    const bool drained = queue.receive(&received_first, sizeof(received_first));
    worker.join();
    css223::ipc::RequestMessage received_second{};
    const bool received = sent && queue.receive(&received_second, sizeof(received_second));
    require(signal_result == 0 && signal_count > previous_signal_count,
            "signal reached waiting sender");
    require(drained && received_first.request_id == 1 && sent && received &&
                received_second.request_id == 2,
            "send retries after interrupted system call");
}

} // namespace

int main() {
    struct sigaction action {};
    action.sa_handler = handle_signal;
    ::sigemptyset(&action.sa_mask);
    struct sigaction previous_action {};
    if (::sigaction(SIGUSR1, &action, &previous_action) != 0) {
        return EXIT_FAILURE;
    }

    try {
        test_interrupted_receive();
        test_interrupted_send();
    } catch (const std::exception& error) {
        std::cerr << "EINTR integration test failed: " << error.what() << '\n';
        ::sigaction(SIGUSR1, &previous_action, nullptr);
        return EXIT_FAILURE;
    }
    ::sigaction(SIGUSR1, &previous_action, nullptr);
    return EXIT_SUCCESS;
}
