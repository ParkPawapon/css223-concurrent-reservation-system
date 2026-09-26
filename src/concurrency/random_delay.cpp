#include "concurrency/random_delay.hpp"

#include <algorithm>
#include <chrono>
#include <random>
#include <thread>

namespace css223::concurrency {

namespace {

std::mt19937& get_thread_local_generator() {
    thread_local std::random_device rd;
    thread_local std::mt19937 generator(rd());
    return generator;
}

} // namespace

RandomDelayGenerator::RandomDelayGenerator(unsigned int min_ms, unsigned int max_ms) noexcept
    : min_ms_(std::min(min_ms, max_ms)), max_ms_(std::max(min_ms, max_ms)) {}

unsigned int RandomDelayGenerator::generate_delay_duration() const {
    return generate_delay_duration(min_ms_, max_ms_);
}

unsigned int RandomDelayGenerator::generate_delay_duration(unsigned int min_ms,
                                                           unsigned int max_ms) {
    unsigned int lower = std::min(min_ms, max_ms);
    unsigned int upper = std::max(min_ms, max_ms);

    if (upper == 0) {
        return 0;
    }

    std::uniform_int_distribution<unsigned int> distribution(lower, upper);
    return distribution(get_thread_local_generator());
}

void RandomDelayGenerator::execute_delay() const {
    execute_delay(min_ms_, max_ms_);
}

void RandomDelayGenerator::execute_delay(unsigned int min_ms, unsigned int max_ms) {
    unsigned int delay_duration = generate_delay_duration(min_ms, max_ms);
    if (delay_duration > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_duration));
    }
}

void RandomDelayGenerator::set_bounds(unsigned int min_ms, unsigned int max_ms) noexcept {
    min_ms_ = std::min(min_ms, max_ms);
    max_ms_ = std::max(min_ms, max_ms);
}

} // namespace css223::concurrency
