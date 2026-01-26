# Episode 01: Hello Vulkan Compute
# Introduction to Vulkan - no shaders, just initialization

set(EPISODE_NAME ep01_hello_compute)

add_executable(${EPISODE_NAME}
    main.c
)

# Link directly to Vulkan - no helper library for this intro episode
target_link_libraries(${EPISODE_NAME} PRIVATE
    Vulkan::Vulkan
)
