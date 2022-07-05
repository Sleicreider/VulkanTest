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
	glm::vec2 tex;	//texture cooreds (u, v)
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

static VkCommandBuffer beginCommandbuffer(VkDevice device, VkCommandPool commandPool)
{
	// command buffer to hold transfer commands
	VkCommandBuffer commandBuffer;

	//commandbuffer details
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	//allocate command buffer from pool
	vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);


	// info to begin the command buffer record
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;		// we are only using the commandbuffer once, so set up for one time submit

	// begin recording transfer commands
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

static void endAndSubmitCommandbuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer)
{
	// end commands
	vkEndCommandBuffer(commandBuffer);

	// queue submission information
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	//submit trasnfer command to transfer queue and wait until it finishes
	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue);

	// free tmp command buffer back to pool
	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

static void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
	VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
	// create buffer
	VkCommandBuffer transferCommandBuffer = beginCommandbuffer(device, transferCommandPool);
	
	//region of data to copy from and to  (copy from start of src to start of dst (0)
	VkBufferCopy bufferCopyRegion{};
	bufferCopyRegion.srcOffset = 0;
	bufferCopyRegion.dstOffset = 0;
	bufferCopyRegion.size = bufferSize;

	//command to copy src buffer to dst buffer
	vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

	endAndSubmitCommandbuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void copyImageBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer srcBuffer, VkImage image, uint32_t width, uint32_t height)
{
	VkCommandBuffer transferCommandBuffer = beginCommandbuffer(device, transferCommandPool);

	VkBufferImageCopy imageRegion{};
	imageRegion.bufferOffset = 0;						// offset into data
	imageRegion.bufferRowLength = 0;					// data spacing - row length of data to calculate data spacing
	imageRegion.bufferImageHeight = 0;					// image height to calculate data spacing
	imageRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // which aspect of image to copy
	imageRegion.imageSubresource.mipLevel = 0;			// mipmap level to copy
	imageRegion.imageSubresource.baseArrayLayer = 0;	// starting array layer (if array)
	imageRegion.imageSubresource.layerCount = 1;		// number of layers to copy, starting at base array layer
	imageRegion.imageOffset = { 0, 0, 0 };				// offset into image (as opposed to raw data in bufferOffset) - start at origin 0,0,0
	imageRegion.imageExtent = { width, height, 1 };		// size of region to copy as (x, y ,z)

	// copy buffer to given image
	vkCmdCopyBufferToImage(transferCommandBuffer, srcBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageRegion); // we used transfer_dst_bit

	endAndSubmitCommandbuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void transitionImageLayout(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = beginCommandbuffer(device, commandPool);

	VkImageMemoryBarrier imageMemoryBarrier{};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.oldLayout = oldLayout;								//transition from
	imageMemoryBarrier.newLayout = newLayout;								//transition to
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;		//dont transfer to different queue  - queue family to transition from
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;		// queue family to transition to
	imageMemoryBarrier.image = image;										// image being accessed and modified as part of barrier
	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	imageMemoryBarrier.subresourceRange.layerCount = 1;
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	imageMemoryBarrier.subresourceRange.levelCount = 1;

	VkPipelineStageFlags srcStage;
	VkPipelineStageFlags dstStage;

	//if transitioning from new image to image ready to received data
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		imageMemoryBarrier.srcAccessMask = 0;							// must happen after (0 means anyhwere)
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // must  happen before (write here) - copy buffertoimage is a transferwrite need to happen before, but start at any time (0)
	
		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;				// anywere above on top of the pipeline
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;					// before transfer write
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		//transition from transfer desitionation to shader readable

		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;		// makes sure its ready before the fragment shader
	}

	vkCmdPipelineBarrier(commandBuffer,
		//pipeline stages - match to src and dst accessmask above
		srcStage,		// srcAccessMask with this stage must happen after
		dstStage,		// dstaccessmas with this state must happen before
		0,		// dependency flags
		0, nullptr,	// memory barrier count and data
		0, nullptr, // buffer memory barrier count and data
		1, &imageMemoryBarrier // image memory barrier, count + data
		);


	endAndSubmitCommandbuffer(device, commandPool, queue, commandBuffer);

}