cmake_minimum_required(VERSION 3.0.0)

project(VulkanTest)

file(GLOB_RECURSE SOURCE Classes/*.cpp)
file(GLOB_RECURSE HEADER Classes/*.h)

set(HEADER ${HEADER} "Thirdparty/stb_image.h")

set(GFLW_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/Thirdparty/GLFW/lib-vc2019/glfw3.lib)
set(GFLW_INCLUDE "Thirdparty/GLFW/include")
find_package(Vulkan REQUIRED)
#find_package(glfw3 REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS} ${GFLW_INCLUDE})

add_definitions(-DPROJ_DIR="${CMAKE_SOURCE_DIR}")
message(STATUS ${CMAKE_SOURCE_DIR})

add_executable(${PROJECT_NAME} ${SOURCE} ${HEADER})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARY} ${GFLW_LIBRARY})