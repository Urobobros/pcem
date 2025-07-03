if(USE_KVM)
    set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
            ${CMAKE_SOURCE_DIR}/includes/private/kvm/kvm.h
    )

    set(PCEM_SRC ${PCEM_SRC}
            kvm/kvm.c
    )

    set(PCEM_DEFINES ${PCEM_DEFINES} USE_KVM)
endif()
