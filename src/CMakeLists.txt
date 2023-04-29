# Add source to this project's executable.
add_executable(VulkanEngine
    main.cpp
    vkEngine.cpp
    vkEngine.h
    vkTypes.h
    vkInitializers.cpp
    vkInitializers.h)

set_property(TARGET VulkanEngine PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:VulkanEngine>")

target_include_directories(VulkanEngine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(VulkanEngine vkbootstrap vma glm tinyobjloader imgui stb_image)

target_link_libraries(VulkanEngine Vulkan::Vulkan sdl2)

add_dependencies(VulkanEngine Shaders)
