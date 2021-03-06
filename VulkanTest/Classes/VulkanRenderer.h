#pragma once

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>
#include "Utilities.h"
#include <set>
#include <algorithm>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Mesh.h"
#include "../Thirdparty/stb_image.h"

class VulkanRenderer
{
public:
	VulkanRenderer();
	~VulkanRenderer() = default;

	int init(GLFWwindow* newWindw);

	void updateModel(int modelId, glm::mat4 newModel);

	void draw();
	void cleanup();

private:
	GLFWwindow* window;
	int currentFrame = 0;


	// scene objects
	//Mesh firstMesh;
	std::vector<Mesh> meshList;

	// scene settings
	struct UboViewProjection
	{
		glm::mat4 projection;
		glm::mat4 view;
	} uboViewProjection;


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

	VkImage depthBufferImage;
	VkDeviceMemory depthBufferMemory;
	VkImageView depthBufferImageView;
	VkFormat depthBufferFormat;

	VkSampler textureSampler;

	// - Descriptors
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSetLayout samplerSetLayout;
	VkPushConstantRange pushConstantRange;

	std::vector<VkBuffer> vpUniformBuffers;
	std::vector<VkDeviceMemory> vpUniformBufferMemory;

	std::vector<VkBuffer> modelDynUniformBuffers;
	std::vector<VkDeviceMemory> modelDynUniformBufferMemory;

	VkDescriptorPool descriptorPool;
	VkDescriptorPool samplerDescriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;
	std::vector<VkDescriptorSet> samplerDescriptorSets;
	/*
		VkDeviceSize minUniformBufferOffset;
		size_t modelUniformAlignment;*/

	//Model* modelTransferSpace;

	//Assets
	std::vector<VkImage> textureImages;
	std::vector<VkDeviceMemory> textureImageMemory; //  could use one with offsets
	std::vector<VkImageView> textureImageViews;

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
	std::vector<VkSemaphore> imageAvailable;
	std::vector<VkSemaphore> renderFinished;
	std::vector<VkFence> drawFences;

	// vulkan functions
	//================================================
	
	// Create functions
	void createInstance();
	void createLogicalDevice();
	void createSurface();
	void createSwapChain();
	void createRenderPass();
	void createDescriptorSetLayout();
	void createPushConstantRange();
	void createGraphicsPipeline();
	void createDepthBufferImage();
	void createFramebuffers();
	void createCommandPool();
	void createCommandBuffers();
	void createSynchronisation();
	void createTextureSampler();
	
	void createUniformBuffers();
	void createDescriptorPool();
	void createDescriptorSets();

	VkFormat getDepthBufferFormat();


	void updateUniformBuffers(uint32_t imageIndex);

	// record functions
	void recordCommands(uint32_t currentImage);

	// get functions
	void getPhysicalDevice();

	//allocate functions
	void allocateDynamicBufferTransferSpace();


	// Support functions

	// checkfunctions
	bool checkInstanceExtensionSupport(const std::vector<const char*>& checkExtensions);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	bool checkDeviceSuitable(VkPhysicalDevice device);

	bool checkValidationLayerSupport();


	VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR chooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
	VkFormat chooseSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags);

	VkImage createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags useFlags, VkMemoryPropertyFlags propFlags, VkDeviceMemory& imageMemory);
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
	VkShaderModule createShaderModule(const std::vector<char>& code);

	int createTextureImage(const std::string& filename);
	int createTexture(const std::string& filename);
	int createTextureDescriptor(VkImageView textureImage);

	//getter functions
	QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);
	SwapChainDetails getSwapChainDetails(VkPhysicalDevice device);


	//loading

	stbi_uc* loadTextureFile(const std::string& filename, int& width, int& height, VkDeviceSize& imageSize);

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