#include <atomic>
#include <cassert>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "common/command.hpp"
#include "common/constants.hpp"
#include "common/result.hpp"
#include "concurrency/random_delay.hpp"
#include "core/reservation_table.hpp"
#include "ipc/message.hpp"
#include "server/request_processor.hpp"
#include "server/server_config.hpp"

namespace {

void test_config_and_delay_bounds() {
    css223::server::ServerConfig config{};
    assert(config.worker_count >= 3);
    assert(config.synchronization_enabled);
    assert(!config.random_delay_enabled);
    assert(config.delay_min_ms == 50);
    assert(config.delay_max_ms == 500);

    css223::concurrency::RandomDelayGenerator generator(50, 500);
    assert(generator.min_delay_ms() == 50);
    assert(generator.max_delay_ms() == 500);

    generator.set_bounds(100, 200);
    assert(generator.min_delay_ms() == 100);
    assert(generator.max_delay_ms() == 200);

    unsigned int duration = generator.generate_delay_duration();
    assert(duration >= 100 && duration <= 200);
}

void test_synchronized_concurrent_reservations() {
    css223::core::ReservationTable table;
    std::mutex mutex;
    css223::server::ServerConfig config{};
    config.synchronization_enabled = true;
    config.random_delay_enabled = true;
    config.delay_min_ms = 5;
    config.delay_max_ms = 15;

    css223::server::RequestProcessor processor(table, mutex, config);

    constexpr int kClientCount = 10;
    std::vector<std::thread> threads;
    threads.reserve(kClientCount);

    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    std::atomic<bool> start_latch{false};

    for (int i = 1; i <= kClientCount; ++i) {
        threads.emplace_back([&, client_id = i]() {
            while (!start_latch.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            css223::ipc::RequestMessage req{};
            req.command = css223::common::CommandType::Reserve;
            req.client_id = static_cast<css223::common::ClientId>(client_id);
            css223::ipc::copy_string_to_buffer(req.seat_id, sizeof(req.seat_id), "A1");

            auto res = processor.process_request(req, static_cast<std::size_t>(client_id));
            if (res.result == css223::common::StatusCode::Success) {
                ++success_count;
            } else {
                ++failure_count;
            }
        });
    }

    // Release all competing threads simultaneously
    start_latch.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    // Mutual exclusion guarantee: exactly one client reserves the seat
    assert(success_count.load() == 1);
    assert(failure_count.load() == kClientCount - 1);

    auto seat = table.get_seat("A1");
    assert(seat.has_value());
    assert(seat->is_reserved());
    assert(seat->owner_client_id().has_value());
}

void test_concurrent_distinct_seat_reservations() {
    css223::core::ReservationTable table;
    std::mutex mutex;
    css223::server::ServerConfig config{};
    config.synchronization_enabled = true;
    config.random_delay_enabled = true;
    config.delay_min_ms = 5;
    config.delay_max_ms = 15;

    css223::server::RequestProcessor processor(table, mutex, config);

    const std::vector<std::string> seat_ids = {"A1", "A2", "B1", "B2", "C1", "C2", "D1", "D2"};
    const std::size_t count = seat_ids.size();

    std::vector<std::thread> threads;
    threads.reserve(count);

    std::atomic<int> success_count{0};
    std::atomic<bool> start_latch{false};

    for (std::size_t i = 0; i < count; ++i) {
        threads.emplace_back([&, idx = i]() {
            while (!start_latch.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            css223::ipc::RequestMessage req{};
            req.command = css223::common::CommandType::Reserve;
            req.client_id = static_cast<css223::common::ClientId>(idx + 1);
            css223::ipc::copy_string_to_buffer(req.seat_id, sizeof(req.seat_id), seat_ids[idx]);

            auto res = processor.process_request(req, idx);
            if (res.result == css223::common::StatusCode::Success) {
                ++success_count;
            }
        });
    }

    start_latch.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    assert(success_count.load() == static_cast<int>(count));
    for (const auto& seat_id : seat_ids) {
        auto seat = table.get_seat(seat_id);
        assert(seat.has_value());
        assert(seat->is_reserved());
    }
}

} // namespace

int main() {
    test_config_and_delay_bounds();
    test_synchronized_concurrent_reservations();
    test_concurrent_distinct_seat_reservations();

    return EXIT_SUCCESS;
}
