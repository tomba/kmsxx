function(check_lto)
    if (DEFINED LTO_WORKS)
        return()
    endif()

    set(LTO_WORKS FALSE CACHE INTERNAL "LTO works")

    CHECK_CXX_COMPILER_FLAG("-flto" HAS_LTO_FLAG)

    if (NOT HAS_LTO_FLAG)
        return()
    endif()

    find_program(LTO_AR NAMES "${CMAKE_C_COMPILER}-ar" gcc-ar)
    find_program(LTO_RANLIB NAMES "${CMAKE_C_COMPILER}-ranlib" gcc-ranlib)

    if (NOT LTO_AR OR NOT LTO_RANLIB)
        return()
    endif()

    EXECUTE_PROCESS(COMMAND "${LTO_AR}" --version RESULT_VARIABLE ret OUTPUT_QUIET ERROR_QUIET)
    if (ret)
        return()
    endif()

    EXECUTE_PROCESS(COMMAND "${LTO_RANLIB}" --version RESULT_VARIABLE ret OUTPUT_QUIET ERROR_QUIET)
    if (ret)
        return()
    endif()

    set(LTO_WORKS TRUE CACHE INTERNAL "LTO works")
endfunction()
