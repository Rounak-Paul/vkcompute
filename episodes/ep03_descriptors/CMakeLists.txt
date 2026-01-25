# Episode 03: Descriptor Sets Deep Dive
# Understanding descriptor management

set(EPISODE_NAME ep03_descriptors)

add_executable(${EPISODE_NAME}
    main.c
)

target_link_libraries(${EPISODE_NAME} PRIVATE
    vkcompute_common
)

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${EPISODE_NAME})
compile_shaders(${EPISODE_NAME} ${SHADER_DIR} ${SHADER_OUTPUT_DIR})
