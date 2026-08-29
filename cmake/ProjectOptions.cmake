include_guard(GLOBAL)

include(CompilerWarnings)
include(Sanitizers)

option(CSS223_ENABLE_CLANG_TIDY "Run clang-tidy while compiling C++ targets" OFF)
option(CSS223_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(CSS223_ENABLE_ADDRESS_SANITIZER "Enable AddressSanitizer" OFF)
option(CSS223_ENABLE_THREAD_SANITIZER "Enable ThreadSanitizer" OFF)
option(CSS223_ENABLE_UNDEFINED_SANITIZER "Enable UndefinedBehaviorSanitizer" OFF)

function(css223_setup_project_options)
    if(TARGET css223_project_options OR TARGET css223_project_warnings)
        message(FATAL_ERROR "CSS223 project option targets have already been created")
    endif()

    add_library(css223_project_options INTERFACE)
    add_library(css223::project_options ALIAS css223_project_options)

    target_compile_features(css223_project_options INTERFACE cxx_std_17)
    css223_enable_sanitizers(css223_project_options)

    add_library(css223_project_warnings INTERFACE)
    add_library(css223::project_warnings ALIAS css223_project_warnings)

    css223_set_project_warnings(css223_project_warnings)
endfunction()

function(css223_apply_project_options target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Cannot apply project options to missing target: ${target_name}")
    endif()

    get_target_property(target_type "${target_name}" TYPE)

    if(target_type STREQUAL "INTERFACE_LIBRARY")
        target_link_libraries("${target_name}" INTERFACE
            css223::project_options
            css223::project_warnings
        )
    else()
        target_link_libraries("${target_name}" PRIVATE
            css223::project_options
            css223::project_warnings
        )

        if(CSS223_ENABLE_CLANG_TIDY)
            find_program(
                CSS223_CLANG_TIDY_EXECUTABLE
                NAMES clang-tidy
                REQUIRED
            )
            set_target_properties("${target_name}" PROPERTIES
                CXX_CLANG_TIDY
                    "${CSS223_CLANG_TIDY_EXECUTABLE};--config-file=${PROJECT_SOURCE_DIR}/.clang-tidy"
            )
        endif()

        set_target_properties("${target_name}" PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
        )
    endif()
endfunction()
