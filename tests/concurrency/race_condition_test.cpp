#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "common/command.hpp"
#include "common/constants.hpp"
#include "common/result.hpp"
#include "common/types.hpp"
#include "core/reservation_table.hpp"
#include "core/seat.hpp"
#include "ipc/message.hpp"
#include "server/request_processor.hpp"
#include "server/server_config.hpp"
#include "server/worker_pool.hpp"

namespace {

void test_synchronized_competing_reservations() {
    std::cout << "[Test 1] Running Synchronized Competing Reservations Test (Mutex ENABLED)...\n";

    css223::core::ReservationTable table;
    std::mutex reservation_mutex;

    css223::server::ServerConfig config{};
    config.synchronization_enabled = true;
    config.random_delay_enabled = true;
    config.delay_min_ms = 5;
    config.delay_max_ms = 15;

    css223::server::RequestProcessor processor(table, reservation_mutex, config);

    constexpr std::size_t kClientCount = 10;
    std::vector<std::thread> threads;
    threads.reserve(kClientCount);

    std::atomic<bool> start_gate{false};
    std::atomic<std::size_t> success_count{0};
    std::atomic<std::size_t> failure_count{0};
    std::atomic<css223::common::ClientId> winning_client_id{css223::common::kInvalidClientId};

    for (std::size_t i = 0; i < kClientCount; ++i) {
        threads.emplace_back([&, i]() {
            css223::common::ClientId client_id = static_cast<css223::common::ClientId>(100 + i);

            css223::ipc::RequestMessage request{};
            request.command = css223::common::CommandType::Reserve;
            request.client_id = client_id;
            request.request_id = static_cast<std::uint32_t>(i + 1);
            css223::ipc::copy_string_to_buffer(request.seat_id, sizeof(request.seat_id), "A1");

            // Wait for simultaneous start signal
            while (!start_gate.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            css223::ipc::ResponseMessage response = processor.process_request(request, i + 1);

            if (response.result == css223::common::StatusCode::Success) {
                success_count.fetch_add(1, std::memory_order_relaxed);
                winning_client_id.store(client_id, std::memory_order_relaxed);
            } else {
                failure_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Release all competing threads at once
    start_gate.store(true, std::memory_order_release);

    for (auto& th : threads) {
        if (th.joinable()) {
            th.join();
        }
    }

    assert(success_count.load() == 1);
    assert(failure_count.load() == kClientCount - 1);

    auto seat_opt = table.get_seat("A1");
    assert(seat_opt.has_value());
    assert(seat_opt->is_reserved());
    assert(seat_opt->owner_client_id().has_value());
    assert(seat_opt->owner_client_id().value() == winning_client_id.load());

    std::cout << "  -> PASSED: Exactly 1 client (" << winning_client_id.load()
              << ") reserved seat A1. " << (kClientCount - 1)
              << " competing clients were safely rejected.\n";
}

void test_unsynchronized_competing_reservations() {
    std::cout << "[Test 2] Running Unsynchronized Competing Reservations Test (Mutex DISABLED / "
                 "Race Condition)...\n";

    css223::core::ReservationTable table;
    std::mutex dummy_mutex;

    css223::server::ServerConfig config{};
    config.synchronization_enabled = false;
    config.random_delay_enabled = true;
    config.delay_min_ms = 10;
    config.delay_max_ms = 25;

    css223::server::RequestProcessor processor(table, dummy_mutex, config);

    constexpr std::size_t kClientCount = 6;
    std::vector<std::thread> threads;
    threads.reserve(kClientCount);

    std::atomic<bool> start_gate{false};
    std::atomic<std::size_t> success_count{0};
    std::atomic<std::size_t> failure_count{0};

    for (std::size_t i = 0; i < kClientCount; ++i) {
        threads.emplace_back([&, i]() {
            css223::ipc::RequestMessage request{};
            request.command = css223::common::CommandType::Reserve;
            request.client_id = static_cast<css223::common::ClientId>(200 + i);
            request.request_id = static_cast<std::uint32_t>(i + 1);
            css223::ipc::copy_string_to_buffer(request.seat_id, sizeof(request.seat_id), "B2");

            while (!start_gate.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            css223::ipc::ResponseMessage response = processor.process_request(request, i + 1);

            if (response.result == css223::common::StatusCode::Success) {
                success_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                failure_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    start_gate.store(true, std::memory_order_release);

    for (auto& th : threads) {
        if (th.joinable()) {
            th.join();
        }
    }

    // Verify system did not crash, table state is readable, and total responses match
    assert(success_count.load() + failure_count.load() == kClientCount);
    auto seat_opt = table.get_seat("B2");
    assert(seat_opt.has_value());
    assert(seat_opt->is_reserved());

    std::cout << "  -> PASSED: Unsynchronized requests processed with conflict logging. Successes: "
              << success_count.load() << ", Failures/Conflicts: " << failure_count.load() << "\n";
}

void test_concurrent_distinct_seat_reservations() {
    std::cout << "[Test 3] Running Concurrent Distinct Seat Reservations (All 20 Seats)...\n";

    css223::core::ReservationTable table;
    std::mutex reservation_mutex;

    css223::server::ServerConfig config{};
    config.synchronization_enabled = true;
    config.random_delay_enabled = true;
    config.delay_min_ms = 2;
    config.delay_max_ms = 8;

    css223::server::RequestProcessor processor(table, reservation_mutex, config);

    const auto& canonical_seats = css223::common::kCanonicalSeatIds;
    const std::size_t seat_count = canonical_seats.size();

    std::vector<std::thread> threads;
    threads.reserve(seat_count);

    std::atomic<bool> start_gate{false};
    std::atomic<std::size_t> success_count{0};

    for (std::size_t i = 0; i < seat_count; ++i) {
        threads.emplace_back([&, i]() {
            css223::common::ClientId client_id = static_cast<css223::common::ClientId>(300 + i);

            css223::ipc::RequestMessage request{};
            request.command = css223::common::CommandType::Reserve;
            request.client_id = client_id;
            request.request_id = static_cast<std::uint32_t>(i + 1);
            css223::ipc::copy_string_to_buffer(
                request.seat_id, sizeof(request.seat_id), canonical_seats[i]);

            while (!start_gate.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            css223::ipc::ResponseMessage response = processor.process_request(request, i + 1);
            if (response.result == css223::common::StatusCode::Success) {
                success_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    start_gate.store(true, std::memory_order_release);

    for (auto& th : threads) {
        if (th.joinable()) {
            th.join();
        }
    }

    assert(success_count.load() == seat_count);
    assert(table.seat_count() == 20);

    for (std::size_t i = 0; i < seat_count; ++i) {
        auto seat = table.get_seat(canonical_seats[i]);
        assert(seat.has_value());
        assert(seat->is_reserved());
        assert(seat->owner_client_id().value() == static_cast<css223::common::ClientId>(300 + i));
    }

    std::cout
        << "  -> PASSED: All 20 canonical seats concurrently reserved with zero cross-talk.\n";
}

void test_concurrent_reserve_and_cancel_invariants() {
    std::cout << "[Test 4] Running Concurrent Reserve and Cancel Invariant Test...\n";

    css223::core::ReservationTable table;
    std::mutex reservation_mutex;

    css223::server::ServerConfig config{};
    config.synchronization_enabled = true;
    config.random_delay_enabled = false;

    css223::server::RequestProcessor processor(table, reservation_mutex, config);

    // Initial state: reserve C1 for client 401
    css223::ipc::RequestMessage initial_res{};
    initial_res.command = css223::common::CommandType::Reserve;
    initial_res.client_id = 401;
    css223::ipc::copy_string_to_buffer(initial_res.seat_id, sizeof(initial_res.seat_id), "C1");
    auto init_resp = processor.process_request(initial_res, 1);
    assert(init_resp.result == css223::common::StatusCode::Success);

    // 5 threads try to cancel C1 (4 unauthorized clients, 1 authorized client 401)
    std::vector<std::thread> cancel_threads;
    std::atomic<bool> start_gate{false};
    std::atomic<std::size_t> cancel_success_count{0};
    std::atomic<std::size_t> cancel_failure_count{0};

    for (std::size_t i = 0; i < 5; ++i) {
        cancel_threads.emplace_back([&, i]() {
            css223::common::ClientId client_id =
                (i == 2) ? 401 : static_cast<css223::common::ClientId>(500 + i);

            css223::ipc::RequestMessage req{};
            req.command = css223::common::CommandType::Cancel;
            req.client_id = client_id;
            css223::ipc::copy_string_to_buffer(req.seat_id, sizeof(req.seat_id), "C1");

            while (!start_gate.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            auto resp = processor.process_request(req, i + 1);
            if (resp.result == css223::common::StatusCode::Success) {
                cancel_success_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                cancel_failure_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    start_gate.store(true, std::memory_order_release);

    for (auto& th : cancel_threads) {
        if (th.joinable()) {
            th.join();
        }
    }

    assert(cancel_success_count.load() == 1);
    assert(cancel_failure_count.load() == 4);

    auto seat = table.get_seat("C1");
    assert(seat.has_value());
    assert(seat->is_available());
    assert(!seat->owner_client_id().has_value());

    std::cout << "  -> PASSED: Invariants maintained. Only authorized owner successfully cancelled "
                 "seat C1.\n";
}

void test_worker_pool_lifecycle() {
    std::cout << "[Test 5] Running WorkerPool Lifecycle & Concurrency Test...\n";

    constexpr std::size_t kWorkerCount = 4;
    css223::server::WorkerPool pool(kWorkerCount);
    assert(pool.worker_count() == kWorkerCount);
    assert(!pool.is_running());

    std::atomic<int> counter{0};
    pool.start([&](std::size_t worker_id) {
        (void) worker_id;
        for (int i = 0; i < 50; ++i) {
            counter.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    assert(pool.is_running());
    pool.stop();
    assert(!pool.is_running());
    assert(counter.load() == static_cast<int>(kWorkerCount * 50));

    std::cout << "  -> PASSED: WorkerPool spawned " << kWorkerCount
              << " worker threads, completed work, and cleanly joined.\n";
}

} // namespace

int main() {
    std::cout << "==============================================================\n";
    std::cout << "   CSS223 Concurrency & Multi-threading Test Suite            \n";
    std::cout << "==============================================================\n";

    test_synchronized_competing_reservations();
    test_unsynchronized_competing_reservations();
    test_concurrent_distinct_seat_reservations();
    test_concurrent_reserve_and_cancel_invariants();
    test_worker_pool_lifecycle();

    std::cout << "==============================================================\n";
    std::cout << "   ALL CONCURRENCY TESTS PASSED SUCCESSFULLY!                 \n";
    std::cout << "==============================================================\n";

    return EXIT_SUCCESS;
}
