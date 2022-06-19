#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>
#include "Utilities.h"
#include <set>
#include <algorithm>
#include <array>

#include "Mesh.h"

class VulkanRenderer
{
public:
	VulkanRenderer();
	~VulkanRenderer() = default;

	int init(GLFWwindow* newWindw);
	void draw();
	void cleanup();

private:
	GLFWwindow* window;


	// scene objects
	Mesh firstMesh;


private:
	struct MainDevice {
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} mainDevice;


	// main components
	VkQueue graphicsQueue;
	VkQueue presentationQueue;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;

	std::vector<SwapChainImage> swapChainImages;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	std::vector<VkCommandBuffer> commandBuffers;

	// pipeline
	VkPipeline graphicsPipeline;
	VkPipelineLayout pipelineLayout;
	VkRenderPass renderPass;

	// pools
	VkCommandPool graphicsCommandPool;

	//utility components
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	//synchronization
	VkSemaphore imageAvailable;
	VkSemaphore renderFinished;

	// vulkan functions
	//================================================
	
	// Create functions
	void createInstance();
	void createLogicalDevice();
	void createSurface();
	void createSwapChain();
	void createRenderPass();
	void createGraphicsPipeline();
	void createFramebuffers();
	void createCommandPool();
	void createCommandBuffers();
	void createSynchronisation();

	// record functions
	void recordCommands();

	// get functions
	void getPhysicalDevice();


	// Support functions

	// checkfunctions
	bool checkInstanceExtensionSupport(const std::vector<const char*>& checkExtensions);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	bool checkDeviceSuitable(VkPhysicalDevice device);

	bool checkValidationLayerSupport();


	VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR chooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);

	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	VkShaderModule createShaderModule(const std::vector<char>& code);


	//getter functions
	QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);
	SwapChainDetails getSwapChainDetails(VkPhysicalDevice device);

private:
	
	// vulkan components
	//================================================

	VkInstance instance;


	inline static std::vector<const char*> validationLayers { "VK_LAYER_KHRONOS_validation" };

#ifdef SLEI_DEBUG
	constexpr static bool enableValidationLayers = true;
#else
	constexpr static bool enableValidationLayers = false;
#endif
};