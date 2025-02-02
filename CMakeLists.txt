cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(VulkanAnimation)

find_package(Vulkan REQUIRED)

add_subdirectory(./vendor/volk) 
add_subdirectory(./vendor/glfw) 
add_subdirectory(./vendor/glm) 

set(shaderSources
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag)

foreach (sourceFile ${shaderSources}) 
    add_custom_command(
        OUTPUT ${sourceFile}.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${sourceFile} -o ${sourceFile}.spv
        DEPENDS ${sourceFile}
        VERBATIM
        )
    list(APPEND shaderBinaries ${sourceFile}.spv)
endforeach(sourceFile)

add_custom_target(shaders ALL DEPENDS ${shaderBinaries})

add_executable(
    app 
    ./src/device.cpp
    ./src/swapchain.cpp
    ./src/renderer.cpp
    ./src/application.cpp
    ./src/main.cpp) 

target_include_directories( 
    app 
    PRIVATE 
    ${Vulkan_INCLUDE_DIRS}
    ./vendor/cgltf/
    ./vendor/glm/
    ./vendor/glfw/include/ 
    ./vendor/volk 
    ./vendor/imgui/)

add_dependencies(app shaders)

target_link_libraries(
    app 
    PRIVATE 
    volk 
    glfw 
    glm
    ${Vulkan_LIBRARIES})
