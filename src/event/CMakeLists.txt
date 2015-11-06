set(SOURCE
    ${SOURCE}
    event/cc_shared.c
    PARENT_SCOPE)

if(OS_PLATFORM STREQUAL "OS_DARWIN")
    set(SOURCE
        ${SOURCE}
        event/cc_kqueue.c)
elseif(OS_PLATFORM STREQUAL "OS_LINUX")
    set(SOURCE
        ${SOURCE}
        event/cc_epoll.c)
endif()
