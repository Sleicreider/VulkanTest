#pragma once

#ifndef NDEBUG
#define SLEI_DEBUG
#endif

#include <fstream>

constexpr const auto MAX_FRAME_DRAWS = 2;

static inline const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

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