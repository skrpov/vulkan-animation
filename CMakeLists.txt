cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(VulkanAnimation)

find_package(Vulkan REQUIRED)

add_subdirectory(./vendor/volk) 
add_subdirectory(./vendor/glfw) 
add_subdirectory(./vendor/glm) 

add_executable(
    app 
    renderer.cpp
    application.cpp
    main.cpp) 

target_include_directories( 
    app 
    PRIVATE 
    ${Vulkan_INCLUDE_DIRS}
    ./vendor/cgltf/
    ./vendor/glm/
    ./vendor/glfw/include/ 
    ./vendor/volk 
    ./vendor/imgui/)

target_link_libraries(
    app 
    PRIVATE 
    volk 
    glfw 
    glm
    ${Vulkan_LIBRARIES})
