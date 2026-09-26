# Test type: integration
# Register targets here with css223_add_test(... TYPE integration ...).

find_package(Threads REQUIRED)

css223_add_test(
    NAME integration_ipc_smoke_test
    TYPE integration
    SOURCES ipc_message_smoke_test.cpp
    LINK_LIBRARIES reservation_ipc
)

css223_add_test(
    NAME client_integration_test
    TYPE integration
    SOURCES client_integration_test.cpp
    LINK_LIBRARIES reservation_client_lib
)

css223_add_test(
    NAME integration_ipc_queue_test
    TYPE integration
    SOURCES ipc_queue_test.cpp
    LINK_LIBRARIES reservation_ipc
    TIMEOUT 30
)

css223_add_test(
    NAME integration_ipc_eintr_test
    TYPE integration
    SOURCES ipc_eintr_test.cpp
    LINK_LIBRARIES reservation_ipc Threads::Threads
    TIMEOUT 30
)

css223_add_test(
    NAME integration_ipc_cleanup_test
    TYPE integration
    SOURCES ipc_cleanup_test.cpp
    LINK_LIBRARIES reservation_ipc
    TIMEOUT 30
)
target_link_options(integration_ipc_cleanup_test PRIVATE "LINKER:--wrap=mq_unlink")
