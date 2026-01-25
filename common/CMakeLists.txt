# Common Vulkan utilities library

add_library(vkcompute_common STATIC
    src/vk_init.c
    src/vk_utils.c
    src/file_utils.c
)

target_include_directories(vkcompute_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(vkcompute_common PUBLIC
    ${Vulkan_LIBRARIES}
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(vkcompute_common PUBLIC)
elseif(APPLE)
    if(MOLTENVK_LIBRARY)
        target_link_libraries(vkcompute_common PUBLIC ${MOLTENVK_LIBRARY})
    endif()
elseif(UNIX)
    target_link_libraries(vkcompute_common PUBLIC m dl pthread)
endif()
