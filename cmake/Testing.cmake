include_guard(GLOBAL)

function(css223_add_test)
    set(supported_test_types
        unit
        integration
        concurrency
    )

    set(options)
    set(one_value_arguments NAME TYPE TIMEOUT)
    set(multi_value_arguments SOURCES LINK_LIBRARIES LABELS COMMAND_ARGUMENTS)

    cmake_parse_arguments(
        PARSE_ARGV 0
        CSS223_TEST
        "${options}"
        "${one_value_arguments}"
        "${multi_value_arguments}"
    )

    if(CSS223_TEST_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "Unknown arguments passed to css223_add_test: "
            "${CSS223_TEST_UNPARSED_ARGUMENTS}"
        )
    endif()

    if(CSS223_TEST_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR
            "Missing values for css223_add_test arguments: "
            "${CSS223_TEST_KEYWORDS_MISSING_VALUES}"
        )
    endif()

    if(NOT CSS223_TEST_NAME)
        message(FATAL_ERROR "css223_add_test requires NAME")
    endif()

    if(NOT CSS223_TEST_TYPE)
        message(FATAL_ERROR "css223_add_test requires TYPE")
    endif()

    if(NOT CSS223_TEST_SOURCES)
        message(FATAL_ERROR "css223_add_test requires at least one source in SOURCES")
    endif()

    list(FIND supported_test_types "${CSS223_TEST_TYPE}" test_type_index)
    if(test_type_index EQUAL -1)
        message(FATAL_ERROR
            "Unsupported test TYPE '${CSS223_TEST_TYPE}'. "
            "Expected one of: ${supported_test_types}"
        )
    endif()

    foreach(additional_label IN LISTS CSS223_TEST_LABELS)
        list(FIND supported_test_types "${additional_label}" reserved_label_index)
        if(NOT reserved_label_index EQUAL -1
           AND NOT additional_label STREQUAL CSS223_TEST_TYPE)
            message(FATAL_ERROR
                "Test '${CSS223_TEST_NAME}' cannot use reserved label "
                "'${additional_label}' with TYPE '${CSS223_TEST_TYPE}'"
            )
        endif()
    endforeach()

    if(TARGET "${CSS223_TEST_NAME}")
        message(FATAL_ERROR "Test target already exists: ${CSS223_TEST_NAME}")
    endif()

    if(NOT CSS223_TEST_TIMEOUT)
        set(CSS223_TEST_TIMEOUT 60)
    endif()

    if(NOT CSS223_TEST_TIMEOUT MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "Test TIMEOUT must be a positive integer")
    endif()

    add_executable("${CSS223_TEST_NAME}" ${CSS223_TEST_SOURCES})
    css223_apply_project_options("${CSS223_TEST_NAME}")

    if(CSS223_TEST_LINK_LIBRARIES)
        target_link_libraries("${CSS223_TEST_NAME}" PRIVATE ${CSS223_TEST_LINK_LIBRARIES})
    endif()

    add_test(
        NAME "${CSS223_TEST_NAME}"
        COMMAND "$<TARGET_FILE:${CSS223_TEST_NAME}>" ${CSS223_TEST_COMMAND_ARGUMENTS}
    )

    set(test_labels "${CSS223_TEST_TYPE}" ${CSS223_TEST_LABELS})
    list(REMOVE_DUPLICATES test_labels)

    set_tests_properties("${CSS223_TEST_NAME}" PROPERTIES
        LABELS "${test_labels}"
        TIMEOUT "${CSS223_TEST_TIMEOUT}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
endfunction()
