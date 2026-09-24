# Test type: concurrency
# Register targets here with css223_add_test(... TYPE concurrency ...).

css223_add_test(
    NAME concurrency_skeleton_smoke_test
    TYPE concurrency
    SOURCES concurrency_skeleton_test.cpp
    LINK_LIBRARIES
        reservation_concurrency
        reservation_server_lib
)
