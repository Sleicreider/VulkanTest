#pragma once

#ifndef NDEBUG
#define SLEI_DEBUG
#endif

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