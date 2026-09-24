find_package(Threads REQUIRED)

# Common definitions library (header-only)
add_library(reservation_common INTERFACE)
target_include_directories(reservation_common INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
css223_apply_project_options(reservation_common)

# Core domain model library
add_library(reservation_core STATIC
    core/seat.cpp
    core/reservation_table.cpp
)
target_link_libraries(reservation_core PUBLIC reservation_common)
css223_apply_project_options(reservation_core)

# Concurrency support library
add_library(reservation_concurrency STATIC
    concurrency/random_delay.cpp
)
target_link_libraries(reservation_concurrency PUBLIC
    reservation_common
    Threads::Threads
)
css223_apply_project_options(reservation_concurrency)

# IPC transport library
add_library(reservation_ipc STATIC
    ipc/message.cpp
    ipc/queue_names.cpp
    ipc/posix_message_queue.cpp
)
target_link_libraries(reservation_ipc PUBLIC
    reservation_common
    reservation_core
)
find_library(LIBRT rt)
if(LIBRT)
    target_link_libraries(reservation_ipc PUBLIC ${LIBRT})
endif()
css223_apply_project_options(reservation_ipc)

# Server orchestration library
add_library(reservation_server_lib STATIC
    server/request_processor.cpp
    server/worker_pool.cpp
    server/server.cpp
)
target_link_libraries(reservation_server_lib PUBLIC
    reservation_common
    reservation_core
    reservation_concurrency
    reservation_ipc
    Threads::Threads
)
css223_apply_project_options(reservation_server_lib)

# Client orchestration library
add_library(reservation_client_lib STATIC
    client/command_parser.cpp
    client/client.cpp
)
target_link_libraries(reservation_client_lib PUBLIC
    reservation_common
    reservation_ipc
)
css223_apply_project_options(reservation_client_lib)

# Main executables
add_executable(reservation_server server/main.cpp)
target_link_libraries(reservation_server PRIVATE reservation_server_lib)
css223_apply_project_options(reservation_server)

add_executable(reservation_client client/main.cpp)
target_link_libraries(reservation_client PRIVATE reservation_client_lib)
css223_apply_project_options(reservation_client)
