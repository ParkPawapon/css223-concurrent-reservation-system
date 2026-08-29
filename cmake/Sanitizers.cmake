include_guard(GLOBAL)

function(css223_enable_sanitizers target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Cannot configure sanitizers for missing target: ${target_name}")
    endif()

    if(CSS223_ENABLE_THREAD_SANITIZER
       AND (CSS223_ENABLE_ADDRESS_SANITIZER OR CSS223_ENABLE_UNDEFINED_SANITIZER))
        message(FATAL_ERROR
            "ThreadSanitizer must use a dedicated build and cannot be combined "
            "with AddressSanitizer or UndefinedBehaviorSanitizer in this project"
        )
    endif()

    set(enabled_sanitizers)

    if(CSS223_ENABLE_ADDRESS_SANITIZER)
        list(APPEND enabled_sanitizers address)
    endif()

    if(CSS223_ENABLE_THREAD_SANITIZER)
        list(APPEND enabled_sanitizers thread)
    endif()

    if(CSS223_ENABLE_UNDEFINED_SANITIZER)
        list(APPEND enabled_sanitizers undefined)
    endif()

    if(NOT enabled_sanitizers)
        return()
    endif()

    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        message(FATAL_ERROR
            "The selected sanitizers require GCC or Clang; detected ${CMAKE_CXX_COMPILER_ID}"
        )
    endif()

    list(JOIN enabled_sanitizers "," sanitizer_list)

    target_compile_options("${target_name}" INTERFACE
        "-fsanitize=${sanitizer_list}"
        -fno-omit-frame-pointer
    )
    target_link_options("${target_name}" INTERFACE
        "-fsanitize=${sanitizer_list}"
        -fno-omit-frame-pointer
    )
endfunction()
