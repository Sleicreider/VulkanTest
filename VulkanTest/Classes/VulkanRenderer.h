#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>
#include "Utilities.h"

class VulkanRenderer
{
public:
	VulkanRenderer();
	~VulkanRenderer() = default;

	int init(GLFWwindow* newWindw);
	void cleanup();

private:
	GLFWwindow* window;


private:
	struct MainDevice {
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} mainDevice;

	VkQueue graphicsQueue;

	// vulkan functions
	//================================================
	
	// Create functions
	void createInstance();
	void createLogicalDevice();

	// get functions
	void getPhysicalDevice();


	// Support functions

	// checkfunctions
	bool checkInstanceExtensionSupport(const std::vector<const char*>& checkExtensions);
	bool checkDeviceSuitable(VkPhysicalDevice device);

	//getter functions
	QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);

private:
	
	// vulkan components
	//================================================

	VkInstance instance;
};