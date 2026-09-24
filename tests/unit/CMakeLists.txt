# Test type: unit
# Register targets here with css223_add_test(... TYPE unit ...).

css223_add_test(
    NAME unit_core_smoke_test
    TYPE unit
    SOURCES core_model_smoke_test.cpp
    LINK_LIBRARIES reservation_core
)
