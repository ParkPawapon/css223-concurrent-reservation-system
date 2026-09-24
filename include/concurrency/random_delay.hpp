#ifndef RESERVATION_CONCURRENCY_RANDOM_DELAY_HPP
#define RESERVATION_CONCURRENCY_RANDOM_DELAY_HPP

#include <chrono>

#include "common/constants.hpp"

namespace css223::concurrency {

class RandomDelayGenerator {
public:
    explicit RandomDelayGenerator(unsigned int min_ms = common::kDefaultMinDelayMs,
                                  unsigned int max_ms = common::kDefaultMaxDelayMs) noexcept;

    void execute_delay() const;
    static void execute_delay(unsigned int min_ms, unsigned int max_ms);

    [[nodiscard]] unsigned int min_delay_ms() const noexcept { return min_ms_; }
    [[nodiscard]] unsigned int max_delay_ms() const noexcept { return max_ms_; }

    void set_bounds(unsigned int min_ms, unsigned int max_ms) noexcept;

private:
    unsigned int min_ms_;
    unsigned int max_ms_;
};

} // namespace css223::concurrency

#endif // RESERVATION_CONCURRENCY_RANDOM_DELAY_HPP
