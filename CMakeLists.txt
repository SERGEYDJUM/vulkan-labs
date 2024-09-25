cmake_minimum_required(VERSION 3.15)

project(
	vk-test
	VERSION 0.1.0
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)

# Assuming GLM is provided by Vulkan SDK 
include(FindVulkan)
include_directories(${Vulkan_INCLUDE_DIRS})

find_package(glfw3 CONFIG REQUIRED)

add_executable(
	vk-lab-exec
	src/main.cpp
	src/utils.hpp
)

target_link_libraries(
	vk-lab-exec
	PRIVATE
	glfw
	${Vulkan_LIBRARIES}
)
