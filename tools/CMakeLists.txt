if(WIN32)
    add_executable(check-whpx check-whpx.c pclog-stub.c cpu_state_stub.c
                   ${CMAKE_SOURCE_DIR}/src/whpx.c)
    add_executable(load-bios-whpx load-bios-whpx.c pclog-stub.c cpu_state_stub.c
                   ${CMAKE_SOURCE_DIR}/src/whpx.c)
    find_library(WINHVPLATFORM_LIB WinHvPlatform)
    if(WINHVPLATFORM_LIB)
        target_link_libraries(check-whpx ${WINHVPLATFORM_LIB})
        target_link_libraries(load-bios-whpx ${WINHVPLATFORM_LIB})
    endif()
endif()

install(PROGRAMS compare_logs.py DESTINATION bin)
