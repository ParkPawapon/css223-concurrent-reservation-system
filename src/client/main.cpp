#include <cstdlib>
#include <iostream>
#include <string_view>

#include "client/client.hpp"
#include "common/types.hpp"

namespace {

void print_usage(std::string_view program_name) {
    std::cout << "Usage: " << program_name << " <client_id>\n"
              << "Example:\n"
              << "  " << program_name << " 1\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    int parsed_id = std::atoi(argv[1]);
    if (parsed_id <= 0) {
        std::cerr << "[Client] Error: client_id must be a positive integer.\n";
        return EXIT_FAILURE;
    }

    auto client_id = static_cast<css223::common::ClientId>(parsed_id);

    std::cout << "[Client] Initializing client ID " << client_id << "\n";

    css223::client::Client client(client_id);
    std::cout << "[Client] Reply queue configured: " << client.reply_queue_name() << "\n"
              << "[Client] Foundation initialized successfully.\n";

    return EXIT_SUCCESS;
}
