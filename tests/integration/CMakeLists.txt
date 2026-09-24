# Test type: integration
# Register targets here with css223_add_test(... TYPE integration ...).

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

