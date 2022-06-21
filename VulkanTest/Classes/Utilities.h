#pragma once

#ifndef NDEBUG
#define SLEI_DEBUG
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <fstream>
#include <glm/glm.hpp>

static inline constexpr const auto MAX_FRAME_DRAWS = 2;
static inline constexpr const auto MAX_OBJECTS = 2;

static inline const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

struct Vertex
{
	glm::vec3 pos;	//vertex position (x, y, z)
	glm::vec3 col;	//vertex color (r, g ,b)

};

// indices (locations) of  Queue families (if they exist at all)
struct QueueFamilyIndices
{
	int graphicsFamily = -1;	//location of graphics queue family
	int presentationFamily = -1; // location of presentation queue family

	//check if queue families are valid
	bool isValid() const
	{
		return graphicsFamily >= 0 && presentationFamily >= 0;
	}
};

struct SwapChainDetails
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities; //surface properties , e.g. image size/extent
	std::vector<VkSurfaceFormatKHR> formats;		//surface image formats e.g. RGBA and size of each color
	std::vector<VkPresentModeKHR> presentationModes;
};

struct SwapChainImage
{
	VkImage image;
	VkImageView imageView;
};

static std::vector<char> readFile(const std::string& filename)
{
	//open stream from given file
	//std::ios::binary tells stream to read file as binary
	//std::ios::ate tells stream to start reading from end of file
	std::ifstream file(filename, std::ios::binary | std::ios::ate);

	//check if file stream successfully opened
	if (!file.is_open())
	{
		throw std::runtime_error("failed to open a file!");
	}

	//get current read position and use it to resize the file buffer
	auto fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> fileBuffer(fileSize);
	
	// move read position (seek to) the start of the file
	file.seekg(0);

	//read the file data into the buffer (stream fileSize in total)
	file.read(fileBuffer.data(), fileSize);

	file.close();
	return fileBuffer;
}

static uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
	// get properties of physical device memory
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
	{
		if ((allowedTypes & (1 << i))				// index of memory type must match corresponding bit in allowedTypes
			&& (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)  // desired property bit flags are part of the memory types property flags
		{
			// this memory type is valid, so return its index
			return i;
		}
	}

	//return ??
}


static void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device,VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
	VkMemoryPropertyFlags bufferProperties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	// CREATE VERTEX BUFFER

// info to create a buffer (doesnt include assigning memory)
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bufferSize;								// size of the buffer (size of 1 vertex * number of vertices)
	bufferInfo.usage = bufferUsage;								// multiple types of buffer possible
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;			// similar to swap chain images, can share vertex buffers

	auto result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Vertex buffer");
	}

	//get buffer memory requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

	//allocate memory to buffer
	VkMemoryAllocateInfo memAllocInfo{};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = memRequirements.size;
	memAllocInfo.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, memRequirements.memoryTypeBits,	//inde of memory type on physical device that has required bitflags
		bufferProperties);//VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	// VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : cpu can interact with memory
	// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : allows placement of data straight into buffer after mapping (otherwise have to specify manually)

// allocate memory to VkDeviceMemory
	result = vkAllocateMemory(device, &memAllocInfo, nullptr, &bufferMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocated Vertex Buffer Memory!");
	}

	// allocate memory to given vertex buffer
	vkBindBufferMemory(device, buffer, bufferMemory, 0);  // offset in memory is 0, we dont have anything else the using the memory where we would an offset for
}


static void copyBuffer(VkDevice device, VkQueue trasnferQueue, VkCommandPool transferCommandPool,
	VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
	// command buffer to hold transfer commands
	VkCommandBuffer transferCommandBuffer;

	//commandbuffer details
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = transferCommandPool;
	allocInfo.commandBufferCount = 1;

	//allocate command buffer from pool
	vkAllocateCommandBuffers(device, &allocInfo, &transferCommandBuffer);


	// info to begin the command buffer record
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;		// we are only using the commandbuffer once, so set up for one time submit

	// begin recording transfer commands
	vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

	//region of data to copy from and to  (copy from start of src to start of dst (0)
	VkBufferCopy bufferCopyRegion{};
	bufferCopyRegion.srcOffset = 0;
	bufferCopyRegion.dstOffset = 0;
	bufferCopyRegion.size = bufferSize;

	//command to copy src buffer to dst buffer
	vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

	// end commands
	vkEndCommandBuffer(transferCommandBuffer);


	// queue submission information
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &transferCommandBuffer;

	//submit trasnfer command to transfer queue and wait until it finishes
	vkQueueSubmit(trasnferQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(trasnferQueue);

	// free tmp command buffer back to pool
	vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);
}