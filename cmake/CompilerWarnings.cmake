include_guard(GLOBAL)

function(css223_set_project_warnings target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Cannot configure warnings for missing target: ${target_name}")
    endif()

    set(msvc_warnings
        /W4
        /permissive-
        /w14242
        /w14254
        /w14263
        /w14265
        /w14287
        /w14289
        /w14296
        /w14311
        /w14545
        /w14546
        /w14547
        /w14549
        /w14555
        /w14619
        /w14640
        /w14826
        /w14905
        /w14906
        /w14928
    )

    set(clang_warnings
        -Wall
        -Wextra
        -Wpedantic
        -Wcast-align
        -Wcast-qual
        -Wconversion
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
        -Wmissing-declarations
        -Wnon-virtual-dtor
        -Wnull-dereference
        -Wold-style-cast
        -Woverloaded-virtual
        -Wshadow
        -Wsign-conversion
    )

    set(gcc_warnings
        -Wall
        -Wextra
        -Wpedantic
        -Wcast-align
        -Wcast-qual
        -Wconversion
        -Wdouble-promotion
        -Wduplicated-branches
        -Wduplicated-cond
        -Wformat=2
        -Wlogical-op
        -Wmissing-declarations
        -Wnon-virtual-dtor
        -Wnull-dereference
        -Wold-style-cast
        -Woverloaded-virtual
        -Wshadow
        -Wsign-conversion
        -Wuseless-cast
    )

    if(CSS223_WARNINGS_AS_ERRORS)
        list(APPEND msvc_warnings /WX)
        list(APPEND clang_warnings -Werror)
        list(APPEND gcc_warnings -Werror)
    endif()

    target_compile_options("${target_name}" INTERFACE
        "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:${msvc_warnings}>"
        "$<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:${clang_warnings}>"
        "$<$<COMPILE_LANG_AND_ID:CXX,GNU>:${gcc_warnings}>"
    )
endfunction()
