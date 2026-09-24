#include <cassert>
#include <cstdlib>

#include "concurrency/random_delay.hpp"
#include "server/server_config.hpp"

int main() {
    // 1. Verify ServerConfig defaults
    css223::server::ServerConfig config{};
    assert(config.worker_count >= 3);
    assert(config.synchronization_enabled);
    assert(!config.random_delay_enabled);
    assert(config.delay_min_ms == 50);
    assert(config.delay_max_ms == 500);

    // 2. Verify RandomDelayGenerator bounds
    css223::concurrency::RandomDelayGenerator generator(50, 500);
    assert(generator.min_delay_ms() == 50);
    assert(generator.max_delay_ms() == 500);

    generator.set_bounds(100, 200);
    assert(generator.min_delay_ms() == 100);
    assert(generator.max_delay_ms() == 200);

    return EXIT_SUCCESS;
}
