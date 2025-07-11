if(WIN32)
    add_executable(check-whpx check-whpx.c)
    find_library(WINHVPLATFORM_LIB WinHvPlatform)
    if(WINHVPLATFORM_LIB)
        target_link_libraries(check-whpx ${WINHVPLATFORM_LIB})
    endif()
endif()

install(PROGRAMS compare_logs.py DESTINATION bin)
