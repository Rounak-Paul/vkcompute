# Chapter 09: Subgroups
# Wave/warp-level operations

set(CHAPTER_NAME ch09_subgroups)

add_executable(${CHAPTER_NAME}
    main.c
)

target_link_libraries(${CHAPTER_NAME} PRIVATE
    vkc
)

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${CHAPTER_NAME})
compile_shaders(${CHAPTER_NAME} ${SHADER_DIR} ${SHADER_OUTPUT_DIR})
