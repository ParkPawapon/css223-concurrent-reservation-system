#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include "server/server.hpp"
#include "server/server_config.hpp"

namespace {

css223::server::Server* g_active_server = nullptr;

void signal_handler(int signal) {
    if (g_active_server != nullptr) {
        std::cout << "\n[Server] Signal " << signal
                  << " (SIGINT / Ctrl+C) received. Initiating graceful shutdown...\n";
        g_active_server->stop();
        g_active_server = nullptr;
    }
}

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

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "==============================================================\n"
              << "   CSS223 Cinema Reservation Server Starting                  \n"
              << "==============================================================\n"
              << "  Workers         : " << config.worker_count << "\n"
              << "  Synchronization : " << (config.synchronization_enabled ? "ENABLED" : "DISABLED (--no-sync)") << "\n"
              << "  Random Delay    : " << (config.random_delay_enabled ? "ENABLED (50-500 ms)" : "DISABLED") << "\n"
              << "  Request Queue   : " << config.request_queue_name << "\n"
              << "==============================================================\n";

    css223::server::Server server(config);
    g_active_server = &server;

    if (!server.start()) {
        std::cerr << "[Server] FATAL: Failed to start server and initialize request queue.\n";
        g_active_server = nullptr;
        return EXIT_FAILURE;
    }

    std::cout << "[Server] Ready and waiting for client requests. Press Ctrl+C to terminate.\n";

    server.run();

    g_active_server = nullptr;
    std::cout << "[Server] Shutdown complete. Resources unlinked cleanly.\n";

    return EXIT_SUCCESS;
}

