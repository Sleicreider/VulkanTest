#include "VulkanRenderer.h"

VulkanRenderer::VulkanRenderer()
{
}

int VulkanRenderer::init(GLFWwindow* newWindw)
{
	window = newWindw;

	try
	{
		createInstance();
		createSurface();
		getPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createRenderPass();
		createGraphicsPipeline();
	}
	catch (const std::runtime_error& e)
	{
		printf("ERROR: %s\n", e.what());
		return EXIT_FAILURE;
	}

	return 0;
}

void VulkanRenderer::cleanup()
{
	vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);
	vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);

	for (auto image : swapChainImages)
	{
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
	}


	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);
	vkDestroyInstance(instance, nullptr);
}

void VulkanRenderer::createInstance()
{
	if (enableValidationLayers && !checkValidationLayerSupport())
	{
		throw std::runtime_error("validation layers requested, but not available!");
	}

	//info about the app itself
	//most data here doesn't affect the programm and is for the developer convenience
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan App";				// custom name of the app
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);	// custom version of the app
	appInfo.pEngineName = "No Engine";						// custom engine name
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);		// custom version of the engine
	appInfo.apiVersion = VK_API_VERSION_1_3;				// vulkan version

	//creation info for a vkInstance (vulkan instance)
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	//create list to hold instance extensions
	std::vector<const char*> instanceExtensions;

	//set up extensions the instance will use
	uint32_t glfwExtensionCount = 0;						// glfw may require multiple extensions
	const char** glfwExtensions;							// extensions passed ass array for cstrings

	//get glfw extensions
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	//add glfw extensions to list of extensions
	for (auto i = 0lu; i < glfwExtensionCount; i++)
	{
		instanceExtensions.push_back(glfwExtensions[i]);
	}


	// check instance extensions supported
	if (!checkInstanceExtensionSupport(instanceExtensions))
	{
		throw std::runtime_error("VkInstance does not support required extensions!");
	}

	createInfo.enabledExtensionCount = glfwExtensionCount;
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();


	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		//setup validation layers which the instance will use
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
	}

	//create instance
	auto result = vkCreateInstance(&createInfo, nullptr, &instance);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Vulkan instance!");
	}
}

void VulkanRenderer::createLogicalDevice()
{
	//get the queue family indices for the chosen physical devices
	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	//can have the same value, check if this is the case through set
	std::set<int> queueFamilyIndices{ indices.graphicsFamily, indices.presentationFamily };

	for (auto queueFamilyIndex : queueFamilyIndices)
	{
		//queues the logical devices needs to create and info to do so (only 1 for now)
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamilyIndex;				// the index of the family to create a queue from
		queueCreateInfo.queueCount = 1;										// number of queues to create

		float priority = 1.f;												 // vulkan needs to know priorities to handle multiple queues(1 = highest prio) 
		queueCreateInfo.pQueuePriorities = &priority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	// information to create the logical device
	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());	// number of queue create infos
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();				// list of queue create infos so device can create required queues
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()); // number of enabled logical device extensions
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	// physical device features the logical device will be using
	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	//create the logical device for the given physical device
	auto result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a logical device!");
	}

	// queues are created at the same time as the device
	// so we want a handle to the queues
	// from given logical device of given queue family of given queue index (0 since only one queue), place reference in given queue
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentationFamily, 0, &presentationQueue);
}

void VulkanRenderer::createSurface()
{
	//create surface, creating a surface create info struct, runs the create surface function
	auto result = glfwCreateWindowSurface(instance, window, nullptr, &surface);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: failed to create surface!");
	}
}

void VulkanRenderer::createSwapChain()
{
	//get swap chain details so we can pick best settings
	SwapChainDetails swapChainDetail = getSwapChainDetails(mainDevice.physicalDevice);

	//find optimal swapchain values
	auto surfaceFormat = chooseBestSurfaceFormat(swapChainDetail.formats);
	auto presentMode = chooseBestPresentationMode(swapChainDetail.presentationModes);
	auto extent = chooseSwapExtent(swapChainDetail.surfaceCapabilities);

	// how many images are in the swapchain? get 1 more than the minimum to allow triple buffering
	
	
	uint32_t imageCount = swapChainDetail.surfaceCapabilities.minImageCount + 1;
	// if 0 then no limit
	if (swapChainDetail.surfaceCapabilities.maxImageCount > 0 && swapChainDetail.surfaceCapabilities.maxImageCount < imageCount)
	{
		imageCount = swapChainDetail.surfaceCapabilities.maxImageCount;
	}

	//create information for swap chain
	VkSwapchainCreateInfoKHR swapChainCreateInfo{};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = surface;
	swapChainCreateInfo.imageFormat = surfaceFormat.format;
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapChainCreateInfo.presentMode = presentMode;
	swapChainCreateInfo.imageExtent = extent;
	swapChainCreateInfo.minImageCount = imageCount;
	swapChainCreateInfo.imageArrayLayers = 1;					// number of layers for each image in chain
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // what attachment images will be used as
	swapChainCreateInfo.preTransform = swapChainDetail.surfaceCapabilities.currentTransform; //transform to perform on swap chain images
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;				// how to handle blending images with external graphics (e.g other windows)
	swapChainCreateInfo.clipped = VK_TRUE;				// whether to clip parts of image not in view (behind other window, off screen etc)


	// get queue family indices
	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);
	
	// if graphics and presentation families are different, then swapchain must let images be shared betweeen families
	if (indices.graphicsFamily != indices.presentationFamily)
	{
		//queues to share between
		uint32_t queueFamilyIndices[] =  
		{
			static_cast<uint32_t>(indices.graphicsFamily),
			static_cast<uint32_t>(indices.presentationFamily)
		};

		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // image share handling
		swapChainCreateInfo.queueFamilyIndexCount = 2; // the 2 families mentioned above - number of queues to share images between
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices; // array of queues to share between
	}
	else
	{
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapChainCreateInfo.queueFamilyIndexCount = 0;
		swapChainCreateInfo.pQueueFamilyIndices = nullptr;
	}

	//used for resizing e.z, old swap chain  been destroyed and this one replaces it, then link old one to quickly and over responsibilities
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	// create swap chain
	auto result = vkCreateSwapchainKHR(mainDevice.logicalDevice, &swapChainCreateInfo, nullptr, &swapchain);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: failed to create swapchain!");
	}

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;
	
	//get swap chain images
	uint32_t swapChainImageCount;
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapChainImageCount, nullptr);

	std::vector<VkImage> images(swapChainImageCount);
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapChainImageCount, images.data());
	
	for (auto image : images)
	{
		SwapChainImage swapChainImage;
		swapChainImage.image = image;

		swapChainImage.imageView = createImageView(image, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
		swapChainImages.push_back(swapChainImage);
		//swapChainImage
		//create image view
	}
}

void VulkanRenderer::createRenderPass()
{
	//color attachment of the render pass
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapChainImageFormat;			// format to use for attachment
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;		// number of samples to write for multisampling
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;	// describes what to do with attachment before rendering
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // describe what to do with attachment after rendering
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// describes what to do with stencil before rendering
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;	// describes what to do with stencil after rendering

	// framebuffer data will be stored as an image, but images can be given different data layouts
	// to give optimal use for certain operations
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;	//image data layout before render pass starts
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;	// image data layout after render pass (to change to)


	// attachment reference uses a attachment index that refers to a index in the attachment list passed to the renderPassCreateInfo
	VkAttachmentReference colorAttachmentReference{};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// information about a particular subpass the render pass is using
	VkSubpassDescription subpass {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // pipeline type subpass is to be bound to
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;
	
	// need to determine when layout transitions occur using subpass dependencies
	std::array<VkSubpassDependency, 2> subpassDependencies;

	// conversion from vk_image_layout_undefined to vk_image_layout_color_attachment_optimal
	//transition must happen after this here
	//must happen after we read from it
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;				// VK_SUBPASS_EXTERNAL  = special value meaning outside of renderpass (c++ code etc)
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;	// pipeline stage
	subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;	// stage access mask

	// but must happen before this here
	// must happen before we read&write from it
	subpassDependencies[0].dstSubpass = 0;
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[0].dependencyFlags = 0;

	// conversion from vk_image_layout_color_attachment_optimal to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	//transition must happen after this here
	subpassDependencies[1].srcSubpass = 0;				
	subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	
	subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	// but must happen before this here
	subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	subpassDependencies[1].dependencyFlags = 0;

	//create info for renderpass
	VkRenderPassCreateInfo renderPassCreateInfo{};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &colorAttachment;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
	renderPassCreateInfo.pDependencies = subpassDependencies.data();

	auto result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a render pass!");
	}
}

void VulkanRenderer::createGraphicsPipeline()
{
	//read in SPIR-V code of shaders
	auto vertexShaderCode = readFile(std::string(PROJ_DIR) + "/Shaders/vert.spv");
	auto fragmentShaderCode = readFile(std::string(PROJ_DIR) + "/Shaders/frag.spv");

	//build shader modules to link to graphics pipeline
	auto vertexShaderModule = createShaderModule(vertexShaderCode);
	auto fragmentShaderModule = createShaderModule(fragmentShaderCode);

	// Shader State Creation 
	//====================================

	//Vertex
	VkPipelineShaderStageCreateInfo vertexShaderCreateInfo{};
	vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderCreateInfo.module = vertexShaderModule;
	vertexShaderCreateInfo.pName = "main";			//starting function name in the shaders void main() in our case

	//Fragment
	VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo{};
	fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderCreateInfo.module = fragmentShaderModule;
	fragmentShaderCreateInfo.pName = "main";			//starting function name in the shaders void main() in our case

	// put em into a array which is required for the graphics pipeline
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };
	

	//Vertex Input
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;	// list of vertex binding description (data spacing / stride information)
	vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;  // list vertex attribute descroptions

	//input assembly
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;		// primitive type to assembly vertices as
	inputAssembly.primitiveRestartEnable = VK_FALSE;			// allow overriding of "stip" topology

	//viewport & scissor
	
	// create a viewport info struct
	VkViewport viewport{};
	viewport.x = 0.f;			// x start coordinate
	viewport.y = 0.f;			// y start coordinate
	viewport.width = (float)swapChainExtent.width;			
	viewport.height = (float)swapChainExtent.height;
	viewport.minDepth = 0.f;			//min framebuffer depth
	viewport.maxDepth = 1.f;			//max framebuffer depth

	//create a scissor info struct
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;

	VkPipelineViewportStateCreateInfo viewPortStateCreateInfo{};
	viewPortStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewPortStateCreateInfo.viewportCount = 1;
	viewPortStateCreateInfo.pViewports = &viewport;
	viewPortStateCreateInfo.scissorCount = 1;
	viewPortStateCreateInfo.pScissors = &scissor;

	
	//// DYNAMIC STATE
	//// dynamic states to enable
	//std::vector<VkDynamicState> dynamicStateEnables;
	//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);	 //dynamic viewport : can resize in commandbuffer vkCmdSetViewport
	//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);	//dynamic scissor : can resize in commandbuffer vkCmdSetScissor

	////dynamic state creation info
	//VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
	//dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	//dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
	//dynamicStateCreateInfo.pDynamicStates = dynamicStateEnables.data();

	// Rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo{};
	rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;	// change if fragments beyond near / far planes are clipped or clamped to plane - need to enable device feature depthClamp if true
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;	//whether to discard data and skip rasterizer, never creates fragments, only suitable for pipleline without framebuffer output
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;	// how to handle filling points between vertices
	rasterizerCreateInfo.lineWidth = 1.f;						// how thick lines should be when drawn
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;		// which face of a triangle to cull
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;	// winding to determine which side is front
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;			// whether to add depth boas to fragments (good for stopping "shadow acne" in shadow mapping)


	// multisampling
	VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo{};
	multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;
	multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;	// number of samples to use per fragment

	//blending
	
	//blend attachment state (how blending is handled)
	VkPipelineColorBlendAttachmentState colorState{};
	colorState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT; // colors to apply blending to
	colorState.blendEnable = VK_TRUE; // enable blending
	
	//blending uses equation (srcColorBlendFactor * newColor) colorblendOp (dstColorBlendFactor * old color)
	colorState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorState.colorBlendOp = VK_BLEND_OP_ADD;

	// summarized (VK_BLEND_FACTOR_SRC_ALPHA * newColor ) + (VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA * oldColor)
	//             (new color alpha * new color) + ((1 - new color alpha) * old color)
	colorState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorState.alphaBlendOp = VK_BLEND_OP_ADD;
	//summaraized:  (1 * new alpha) + (0 * old alpha) = new alpha


	//blending decides how to blend a new color being written to a fragment, with the old value
	VkPipelineColorBlendStateCreateInfo colorBlendingCreateInfo{};
	colorBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendingCreateInfo.logicOpEnable = VK_FALSE;			// alternative to calculations is to use logical operations
	colorBlendingCreateInfo.attachmentCount = 1;
	colorBlendingCreateInfo.pAttachments = &colorState;


	// pipeline layout (todo: apply future descriptor set layouts)
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 0;
	pipelineLayoutCreateInfo.pSetLayouts = nullptr;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	//create pipeline layout
	auto result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create pipeline layout");
	}

	// depth stencil testing
	// TODO: set up depth stencil testing



	// Create Pipeline
	//=================================

	VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;					// number of shader stages
	pipelineCreateInfo.pStages = shaderStages;
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;			// all the fixed pipeline stats
	pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
	pipelineCreateInfo.pViewportState = &viewPortStateCreateInfo;
	pipelineCreateInfo.pDynamicState = nullptr;
	pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendingCreateInfo;
	pipelineCreateInfo.pDepthStencilState = nullptr;
	pipelineCreateInfo.layout = pipelineLayout;		// pipeline layout the pipeline should use
	pipelineCreateInfo.renderPass = renderPass;		// renderpass description the pipeline is compatible with
	pipelineCreateInfo.subpass = 0;					// subpass of render pass to use with the pipeline

	// create graphics pipeline
	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create graphics pipeline!");
	}

	//pipeline derivatives , can create multiple pipelines that derive from one another for optimization
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;	// existing pipeline to derive from
	pipelineCreateInfo.basePipelineIndex = -1;				// or index of pipeline being created to derive from ( in case create multiple at once)

	// Destroy shader modules - no longer neaded after pipeline was created
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);
}

void VulkanRenderer::getPhysicalDevice()
{
	// enumerate physical devices the vkinstance can access
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	//if no devices are available, then none support vulkan
	if (deviceCount == 0)
	{
		throw std::runtime_error("can't find gpus that support the vulkan instance");
	}

	// get list of physical devices
	std::vector<VkPhysicalDevice> deviceList(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data());

	//pick a suitable device
	for (const auto& device : deviceList)
	{
		if (checkDeviceSuitable(device))
		{
			mainDevice.physicalDevice = device;
			break;
		}
	}
}

bool VulkanRenderer::checkInstanceExtensionSupport(const std::vector<const char*>& checkExtensions)
{
	uint32_t extensionCount = 0;

	//get number of extensions
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	//create a list of vkExtensionProperties using the count

	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	//check if given extensions are in list of available extensions
	for (auto checkExtension : checkExtensions)
	{
		auto hasExtension = false;
		for (const auto& extension : extensions)
		{
			if (strcmp(checkExtension, extension.extensionName) == 0)
			{
				hasExtension = true;
				break;
			}
		}

		if (!hasExtension)
		{
			return false;
		}
	}

	return true;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	if (extensionCount == 0)
	{
		return false;
	}

	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

	//check for extension
	for (const auto& deviceExtension : deviceExtensions)
	{
		auto haseExtension = false;
		for (const auto extension : extensions)
		{
			if (strcmp(deviceExtension, extension.extensionName) == 0)
			{
				haseExtension = true;
				break;
			}
		}

		if (!haseExtension)
		{
			return false;
		}
	}

	return true;
}

bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device)
{
	//// information about the device itself
	//VkPhysicalDeviceProperties deviceProperties;
	//vkGetPhysicalDeviceProperties(device, &deviceProperties);

	////information about what the device can do (geo shader , tessalation shader, wide lines, etc)
	//VkPhysicalDeviceFeatures deviceFeatures;
	//vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	auto extensionSupported = checkDeviceExtensionSupport(device);
	auto swapChainValid = false;
	
	if (extensionSupported)
	{
		SwapChainDetails swapChainDetails = getSwapChainDetails(device);
		swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
	}

	QueueFamilyIndices indices = getQueueFamilies(device);
	return indices.isValid() && extensionSupported  && swapChainValid;
}

bool VulkanRenderer::checkValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (auto layerName : validationLayers)
	{
		auto layerFound = false;

		for (auto layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
		{
			return false;
		}
	}

	return true;
}

//best format is subjective but ours will be:
// format: VK_FORMAT_R8B8G8A8_UNORM
// color:  VK_COLOR_SPACE_SRBG_NONLINEAR_KHR (backup VK_FORMAT_B8G8R8A8_UNORM)
VkSurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
	{
		return { VK_FORMAT_R8G8B8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
	}

	for (const auto& format : formats)
	{
		if ((format.format == VK_FORMAT_R8G8B8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM) 
			&& format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}

	//just return first format if non other found
	return formats[0];
}

VkPresentModeKHR VulkanRenderer::chooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes)
{
	for (const auto& presentationMode : presentationModes)
	{
		if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return presentationMode;
		}
	}

	//always is available from vulkan spec
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR & surfaceCapabilities)
{
	//if current extend is at numeric limits, this means the extend can vary, otherwise it is the size of the window.
	if (surfaceCapabilities.currentExtent.height != std::numeric_limits<uint32_t>::max())
	{
		return surfaceCapabilities.currentExtent;
	}

	// if value can vary, need to set manually

	//get window size
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);

	VkExtent2D newExtent{};
	newExtent.width = static_cast<uint32_t>(width);
	newExtent.height = static_cast<uint32_t>(height);

	// surface also defines max and min, so make sure it is within the boundries by clamping the values
	newExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
	newExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));

	return newExtent;
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo viewCreateInfo{};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.image = image;					// image to create info for
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; //type of image
	viewCreateInfo.format = format;					// format of image data
	
	// allows remapping of rgba components to other rgba values
	viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	
	
	//subresources allow the view to view only a part of an image
	viewCreateInfo.subresourceRange.aspectMask = aspectFlags;  //which aspect of image to view (e.g. color bit of viewing color
	viewCreateInfo.subresourceRange.baseMipLevel = 0;		//start mipmap level to view from
	viewCreateInfo.subresourceRange.levelCount = 1;			//how many from the mipmalevels to view
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;			//start array level to view from
	viewCreateInfo.subresourceRange.layerCount = 1;			//number of array levels to view

	//create image and return it
	VkImageView imageView;
	auto result = vkCreateImageView(mainDevice.logicalDevice, &viewCreateInfo, nullptr, &imageView);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create an image view!");
	}

	return imageView;

	//return viewCreateInfo;
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code)
{
	//shadermodule creation information
	VkShaderModuleCreateInfo shaderModuleCreateInfo{};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = code.size();
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
	
	VkShaderModule shaderModule;
	auto result = vkCreateShaderModule(mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("faild to create shadermodule!");
	}

	return shaderModule;
}

QueueFamilyIndices VulkanRenderer::getQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	//get all queue family property info for the given device
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

	for (auto i = 0ul; i < queueFamilyCount; i++)
	{
		// queue family has at least 1 queue in that family, queue can have multiple types define trhough a bitfield, bitwise and to check if it has the one required
		auto& queueFamily = queueFamilyList[i];
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			// found valid family
			indices.graphicsFamily = i; // if queue family is valid, then get the index
		}

		VkBool32 presentationSupport = false;
		auto result = vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);

		if (queueFamily.queueCount > 0 && presentationSupport)
		{
			indices.presentationFamily = i;
		}

		if (indices.isValid())
		{
			break;
		}
	}

	return indices;
}

SwapChainDetails VulkanRenderer::getSwapChainDetails(VkPhysicalDevice device)
{
	SwapChainDetails swapChainDetails;

	// CAPABILITIES
	//============================

	//get the surface capabilities for the given surface on the given physical device
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapChainDetails.surfaceCapabilities);

	// FORMATS
	//============================

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);


	// if format returend get list of formats
	if (formatCount != 0)
	{
		swapChainDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapChainDetails.formats.data());
	}

	// PRESENTATION MODE
	//============================

	uint32_t presentationCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, nullptr);

	// if presentation modes returned, get list of presentation modes
	if (presentationCount != 0)
	{
		swapChainDetails.presentationModes.resize(presentationCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, swapChainDetails.presentationModes.data());
	}

	return swapChainDetails;
}
