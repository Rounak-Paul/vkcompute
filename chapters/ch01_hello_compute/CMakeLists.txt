# Chapter 01: Hello Vulkan Compute
# Introduction to Vulkan - no shaders, just initialization

set(CHAPTER_NAME ch01_hello_compute)

add_executable(${CHAPTER_NAME}
    main.c
)

# Link directly to Vulkan - no helper library for this intro chapter
target_link_libraries(${CHAPTER_NAME} PRIVATE
    Vulkan::Vulkan
)
