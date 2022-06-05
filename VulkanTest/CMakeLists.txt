cmake_minimum_required(VERSION 3.0.0)

project(VulkanTest)

set(GFLW_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/Thirdparty/GLFW/lib-vc2019/glfw3.lib)
set(GFLW_INCLUDE "Thirdparty/GLFW/include")
find_package(Vulkan REQUIRED)
#find_package(glfw3 REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS} ${GFLW_INCLUDE})

add_executable(${PROJECT_NAME} Classes/main.cpp)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_20)
target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARY} ${GFLW_LIBRARY})