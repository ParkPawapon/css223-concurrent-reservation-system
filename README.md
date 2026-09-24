# Concurrent Cinema Seat Reservation System

## Overview
This repository contains the Concurrent Cinema Seat Reservation System developed for the CSS223 Operating Systems course. The project demonstrates inter-process communication (IPC), multi-threaded synchronization, mutual exclusion, and controlled race-condition experiments across independent client processes and a multi-threaded server.

## Architecture
The system employs a client-server architecture built on Linux OS primitives:
- **Clients**: Independent processes submitting seat reservation requests.
- **Server**: Multi-threaded process managing the shared reservation table using a worker thread pool.
- **IPC Mechanism**: POSIX Message Queues for request and response message exchange.
- **Shared State**: In-memory `ReservationTable` protected by a global `std::mutex`.

## Requirements
- Linux (Ubuntu 24.04 or compatible) or Docker
- C++17 compatible compiler (GCC >= 13 or Clang >= 17)
- CMake >= 3.28
- Ninja build system
- POSIX Real-Time Extensions (`librt`) for POSIX Message Queues

## Build
Configure and build using CMake presets:

```bash
# Configure debug build
cmake --preset debug

# Build targets
cmake --build --preset debug
```

Available executables generated in the build directory:
- `reservation_server`
- `reservation_client`

## Run Server
Start the reservation server with default configuration (3 workers, synchronization enabled):

```bash
./build/debug/src/reservation_server
```

CLI options:
- `--workers <N>`: Set worker thread count (default: 3)
- `--no-sync`: Disable mutex synchronization (for Experiment 2)
- `--delay`: Enable random delay (50–500 ms) between check and update
- `--help`: Display usage information

## Run Clients
Run independent client instances with a unique client ID:

```bash
./build/debug/src/reservation_client <client_id>
```

<!-- TODO: Implement interactive client command loop in application behavior phase -->

## Supported Commands
The reservation protocol defines five core commands:
- `LIST`: Retrieve current reservation status of all 20 seats
- `STATUS <seat_id>`: Query status of an individual seat
- `RESERVE <seat_id>`: Request reservation of a seat
- `CANCEL <seat_id>`: Cancel an existing reservation owned by the client
- `QUIT`: Terminate client session

## POSIX Message Queue
- **Server Request Queue**: Central queue for incoming client requests (`/css223_reservation_requests`).
- **Client Reply Queue**: Dedicated per-client reply queue (`/css223_client_<client_id>_reply`).
- Message payloads use fixed-size, trivially copyable structures suitable for raw IPC transfer.

## Synchronization
- Mutual exclusion is enforced via a single global `std::mutex` protecting the `ReservationTable`.
- Read operations (`LIST`, `STATUS`) take atomic state snapshots before releasing the lock to format responses.
- Write operations (`RESERVE`, `CANCEL`) maintain atomic state transitions (CHECK → DELAY → UPDATE).

## Experiment Modes
The architecture provides structural configuration for the course experiments:
- **Experiment 1 (Sequential Baseline)**: 1 worker thread.
- **Experiment 2 (Concurrent Unsynchronized)**: >= 3 worker threads, synchronization disabled (`--no-sync`), random delay (50–500 ms) enabled (`--delay`).
- **Experiment 3 (Concurrent Synchronized)**: >= 3 worker threads, synchronization enabled, random delay (50–500 ms) enabled (`--delay`).

<!-- TODO: Execute experiment runs and collect logs/screenshots in later phase -->

## Docker
The repository includes a multi-stage Dockerfile and Docker Compose configuration:

```bash
# Build and run development environment
docker compose up -d development
```

## Testing
Tests are organized into three tiers via CTest:
- `unit`: Core domain models and parsing
- `integration`: IPC message layout and queue naming
- `concurrency`: Concurrency configuration and delay generator

Run test suites using workflow presets:

```bash
cmake --workflow --preset workflow-test-unit
cmake --workflow --preset workflow-test-integration
cmake --workflow --preset workflow-test-concurrency
```
