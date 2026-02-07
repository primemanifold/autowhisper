function(autowhisper_set_warnings target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
            -Wall -Wextra -Wpedantic
            -Wno-unused-parameter
            -Wno-missing-field-initializers
        >
        $<$<CXX_COMPILER_ID:MSVC>:
            /W4 /wd4100 /wd4244 /wd4267
        >
    )
endfunction()
