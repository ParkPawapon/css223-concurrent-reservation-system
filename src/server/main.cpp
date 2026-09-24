#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include "server/server.hpp"
#include "server/server_config.hpp"

namespace {

void print_usage(std::string_view program_name) {
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "  --workers <N>   Set worker thread count (default: 3)\n"
              << "  --no-sync       Disable mutex synchronization (Experiment 2)\n"
              << "  --delay         Enable random delay (50-500 ms) between check and update\n"
              << "  --help          Display this help message\n";
}

} // namespace

int main(int argc, char* argv[]) {
    css223::server::ServerConfig config{};

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--help") {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        if (arg == "--no-sync") {
            config.synchronization_enabled = false;
        } else if (arg == "--delay") {
            config.random_delay_enabled = true;
        } else if (arg == "--workers" && i + 1 < argc) {
            int count = std::atoi(argv[++i]);
            if (count > 0) {
                config.worker_count = static_cast<std::size_t>(count);
            }
        }
    }

    std::cout << "[Server] Initializing reservation server\n"
              << "  Workers: " << config.worker_count << "\n"
              << "  Synchronization: " << (config.synchronization_enabled ? "ENABLED" : "DISABLED")
              << "\n"
              << "  Random Delay: " << (config.random_delay_enabled ? "ENABLED" : "DISABLED")
              << "\n"
              << "  Queue: " << config.request_queue_name << "\n";

    css223::server::Server server(config);

    // Baseline structural verification: confirm startup capability
    std::cout << "[Server] Foundation initialized successfully.\n";

    return EXIT_SUCCESS;
}
