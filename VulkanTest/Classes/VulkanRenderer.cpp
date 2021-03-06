#include "VulkanRenderer.h"

VulkanRenderer::VulkanRenderer()
{
}

int VulkanRenderer::init(GLFWwindow* newWindow)
{
	window = newWindow;

	try
	{
		createInstance();
		createSurface();
		getPhysicalDevice();
		createLogicalDevice();
		createSwapChain();

		depthBufferFormat = getDepthBufferFormat();

		createRenderPass();
		createDescriptorSetLayout();
		createPushConstantRange();
		createGraphicsPipeline();
		createDepthBufferImage();
		createFramebuffers();
		createCommandPool();

		createCommandBuffers();
		createTextureSampler();
		//allocateDynamicBufferTransferSpace();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createSynchronisation();

		uboViewProjection.projection = glm::perspective(glm::radians(45.0f), (float)swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 100.0f);
		uboViewProjection.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		uboViewProjection.projection[1][1] *= -1;

		// Create a mesh

		//vertex data
		std::vector<Vertex> meshVertices =
		{
			{{-0.4, 0.4, 0.0}, {1.f, 0.f, 0.f}, {1.f, 1.f}}, //0
			{{-0.4, -0.4, 0.0}, {1.f, 0.f, 0.f}, {1.f,0.f}} , //1
			{{0.4, -0.4, 0.0}, {1.f, 0.f, 0.f}, {0.f, 0.f}}, //2
			{{0.4, 0.4, 0.0}, {1.f, 0.f, 0.f}, {0.f, 1.f}} , //3
		};

		std::vector<Vertex> meshVertices2 =
		{
			{{-0.4, 0.25, 0.0}, {0.f, 1.f, 0.f}, {1.f, 1.f}}, //0
			{{-0.4, -0.25, 0.0}, {0.f, 1.f, 0.f}, {1.f, 0.f}} , //1
			{{0.4, -0.25, 0.0}, {0.f, 1.f, 0.f}, {0.f, 0.f}}, //2
			{{0.4, 0.25, 0.0}, {0.f, 1.f, 0.f}, {0.f, 1.f}} , //3
		};

		// index data
		std::vector<uint32_t> meshIndices = {
			0, 1, 2,
			2 ,3, 0
		};

		Mesh firstMesh = Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, meshVertices, meshIndices, createTexture("peepo.jpg"));
		Mesh secondMesh = Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, meshVertices2, meshIndices, createTexture("peepo2.jpg"));
		meshList.push_back(firstMesh);
		meshList.push_back(secondMesh);
	}
	catch (const std::runtime_error& e)
	{
		printf("ERROR: %s\n", e.what());
		return EXIT_FAILURE;
	}

	return 0;
}

void VulkanRenderer::updateModel(int modelId, glm::mat4 newModel)
{
	if (modelId >= meshList.size())
		return;

	meshList[modelId].setModel(newModel);
}

void VulkanRenderer::draw()
{
	//1 get next available image to draw to and set something to signal when we're finished with the image (a semaphore)
	//2 submit command buffer to queue for execution, make sure it waits for image to be signaled as available before drawing
	// and signals when it has finished rendering
	//3 present image to screen when it has signaled finished rendering

	//Get next image

	// wait for given fence to signal (open) from last draw before continuing
	vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

	// manually reset (close) fences
	vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]);


	//get index of next image to be drawn to, and signal semaphore when ready to be drawn to
	uint32_t imageIndex;
	vkAcquireNextImageKHR(mainDevice.logicalDevice, swapchain, std::numeric_limits<uint64_t>::max(), imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);

	recordCommands(imageIndex);

	updateUniformBuffers(imageIndex);

	// submit command buffer to render
	//queue submussion info
	VkSubmitInfo submitinfo{};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitinfo.waitSemaphoreCount = 1;					// number of semaphores to wait on
	submitinfo.pWaitSemaphores = &imageAvailable[currentFrame];		// list of semaphores to wait on
	
	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submitinfo.pWaitDstStageMask = waitStages;		//stages to check semaphores at
	submitinfo.commandBufferCount = 1;
	submitinfo.pCommandBuffers = &commandBuffers[imageIndex];	// command buffer to submit
	submitinfo.signalSemaphoreCount = 1;					// number of semaphores to signal
	submitinfo.pSignalSemaphores = &renderFinished[currentFrame];		// semaphores to signal when command buffer finishes

	auto result = vkQueueSubmit(graphicsQueue, 1, &submitinfo, drawFences[currentFrame]);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit command buffer to queue");
	}


	// present rendered image to screen
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinished[currentFrame];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &imageIndex;					// index of images in swapchains to present

	result = vkQueuePresentKHR(presentationQueue, &presentInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to present image!");
	}

	// Get next frame ( use & MAX_FRAME_DRAWS to keep value below MAX_FRAME_DRAWS)
	currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::cleanup()
{
	//wait until no actions being run on device before destroying
	vkDeviceWaitIdle(mainDevice.logicalDevice);

	//_aligned_free(modelTransferSpace);

	vkDestroyDescriptorPool(mainDevice.logicalDevice, samplerDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, samplerSetLayout, nullptr);

	vkDestroySampler(mainDevice.logicalDevice, textureSampler, nullptr);

	for (size_t i = 0; i < textureImages.size(); i++)
	{
		vkDestroyImageView(mainDevice.logicalDevice, textureImageViews[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, textureImages[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, textureImageMemory[i], nullptr);
	}

	vkDestroyImageView(mainDevice.logicalDevice, depthBufferImageView, nullptr);
	vkDestroyImage(mainDevice.logicalDevice, depthBufferImage, nullptr);
	vkFreeMemory(mainDevice.logicalDevice, depthBufferMemory, nullptr);

	vkDestroyDescriptorPool(mainDevice.logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descriptorSetLayout, nullptr);

	for (auto i = 0lu; i < swapChainImages.size(); i++)
	{
		vkDestroyBuffer(mainDevice.logicalDevice, vpUniformBuffers[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, vpUniformBufferMemory[i], nullptr);
/*
		vkDestroyBuffer(mainDevice.logicalDevice, modelDynUniformBuffers[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, modelDynUniformBufferMemory[i], nullptr);*/
	}

	for (auto& mesh : meshList)
	{
		mesh.destroyBuffers();
	}

	for (auto i = 0lu; i < MAX_FRAME_DRAWS; i++)
	{
		vkDestroySemaphore(mainDevice.logicalDevice, renderFinished[i], nullptr);
		vkDestroySemaphore(mainDevice.logicalDevice, imageAvailable[i], nullptr);
		vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
	}

	vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);
	for (auto framebuffer : swapChainFramebuffers)
	{
		vkDestroyFramebuffer(mainDevice.logicalDevice, framebuffer, nullptr);
	}

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
	deviceFeatures.samplerAnisotropy = VK_TRUE;	// Enable Anisotropy
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

	//depth attachment
	VkAttachmentDescription deptchAttachment{};
	deptchAttachment.format = depthBufferFormat;
	deptchAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	deptchAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	deptchAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	deptchAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	deptchAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	deptchAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	deptchAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


	// attachment reference uses a attachment index that refers to a index in the attachment list passed to the renderPassCreateInfo
	VkAttachmentReference colorAttachmentReference{};
	colorAttachmentReference.attachment = 0;		//pos 0 in the array of attachments
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentReference{};
	depthAttachmentReference.attachment = 1;		// pos 1 in the array of attachment used by render pass
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // stays the same as final layout

	// information about a particular subpass the render pass is using
	VkSubpassDescription subpass {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // pipeline type subpass is to be bound to
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;
	subpass.pDepthStencilAttachment = &depthAttachmentReference;
	
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

	std::array<VkAttachmentDescription, 2> renderPassAttachments{ colorAttachment, deptchAttachment }; // order important 0 color, 1 depth

	//create info for renderpass
	VkRenderPassCreateInfo renderPassCreateInfo{};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
	renderPassCreateInfo.pAttachments = renderPassAttachments.data();
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

void VulkanRenderer::createDescriptorSetLayout()
{
	// uniform values descriptor set layout
	// vp binding info
	VkDescriptorSetLayoutBinding vpLayoutBinding{};
	vpLayoutBinding.binding = 0;				// layout(binding=0) in vert shader |binding point in sharder designated by binding number in shader)
	vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // type of descriptor (uniform , dynamic uniform, image sampler, etc)
	vpLayoutBinding.descriptorCount = 1;					// number of descriptors for binding (atm only MVP)
	vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;	//shader stage to bind to (vert in our case)
	vpLayoutBinding.pImmutableSamplers = nullptr;			// for texture: can make sampler unchangeable (immutable) by specifying layout

	//// model bindings
	//VkDescriptorSetLayoutBinding modelLayoutBinding{};
	//modelLayoutBinding.binding = 1;
	//modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	//modelLayoutBinding.descriptorCount = 1;
	//modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	//modelLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 1> layoutBindings{ vpLayoutBinding };//, modelLayoutBinding };

	// create descriptor set layout with given bindings
	VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());	// number of binding infos
	layoutCreateInfo.pBindings = layoutBindings.data();		// array of  binding infos

	//create descriptor set layout
	auto result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Faild to create descriptor set layout!");
	}

	// create texture sampler descriptor set layout

	//texture binding info
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;


	//create descriptor set layout with given bindings for textures
	VkDescriptorSetLayoutCreateInfo textureLayoutCreateInfo{};
	textureLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	textureLayoutCreateInfo.bindingCount = 1;
	textureLayoutCreateInfo.pBindings = &samplerLayoutBinding;

	//create descriptor set layout
	result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &textureLayoutCreateInfo, nullptr, &samplerSetLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Faild to create sampler descriptor set layout!");
	}
}

void VulkanRenderer::createPushConstantRange()
{
	// define pish constnat values , no create needed
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // shader stage push constant where it will go to
	pushConstantRange.offset = 0;							// offset into given data to pas to push constant
	pushConstantRange.size = sizeof(Model);					//size of data being passed
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
	
	// how the data for a single vertex (including info such as pos, color texcoords, normals, etc) is as a whole
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;					// can bind multiple streams of data, this defines which one
	bindingDescription.stride = sizeof(Vertex);		// size of a single vertex object
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;	// how to move between data after each vertex
																// vk_vertex_inuput_rate_vertex : move on to the next vertex
																// vk_vertex_input_rate_instance : move to a vertex for the next instance

	// how the data for an attribute is defined within a vertex
	std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions;

	//Position attribute
	attributeDescriptions[0].binding = 0; // layout(binding=0, location=0) int vec3 pos; its the invisible binding feature | which binding the data is at (should be same as above unles you have more streams)
	attributeDescriptions[0].location = 0; // location in the above mentioned location feature of the shader - location in shader where data will be read from
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT; // format the data will take (also helps define the size of the data)
	attributeDescriptions[0].offset = offsetof(Vertex, pos);		// where this attribute is defined in the data for a single vertex

	// color attribute
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(Vertex, col);

	// texture attribute
	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = offsetof(Vertex, tex);

	//Vertex Input
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;	// list of vertex binding description (data spacing / stride information)
	vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();  // list vertex attribute descroptions

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
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;	// winding to determine which side is front   //we inverted Y make CCW
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


	// pipeline layout

	std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts { descriptorSetLayout, samplerSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

	//create pipeline layout
	auto result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create pipeline layout");
	}

	// depth stencil testing
	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo{};
	depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilCreateInfo.depthTestEnable = VK_TRUE;		// enable checking depth to determine fragment write
	depthStencilCreateInfo.depthWriteEnable = VK_TRUE;			// enable writing to depth buffer to replace old values
	depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS; // is new value less ? then replace - comparison operation that allows an overwrite (is in front)
	depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;	// depth bounds test: does the depth value exist between bounds
	depthStencilCreateInfo.stencilTestEnable = VK_FALSE;		// enable stencil test




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
	pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;
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

void VulkanRenderer::createDepthBufferImage()
{
	// create depth buffer image
	depthBufferImage = createImage(swapChainExtent.width, swapChainExtent.height, depthBufferFormat,
		VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthBufferMemory);

	// create depth buffer image
	depthBufferImageView = createImageView(depthBufferImage, depthBufferFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanRenderer::createFramebuffers()
{
	// resize to the amount of swapchainimages , we create 1 framebuffer per image
	swapChainFramebuffers.resize(swapChainImages.size());

	//create a framebuffer for each swap chain image
	for (auto i = 0lu; i < swapChainFramebuffers.size(); i++)
	{
		std::array<VkImageView, 2> attachments = { swapChainImages[i].imageView, depthBufferImageView }; // order important , color 1, depth 2

		VkFramebufferCreateInfo framebufferCreateInfo{};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = renderPass;						// renderpass layouts the frabuffer will be used with
		framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferCreateInfo.pAttachments = attachments.data();				// list of attachments
		framebufferCreateInfo.width = swapChainExtent.width;
		framebufferCreateInfo.height = swapChainExtent.height;
		framebufferCreateInfo.layers = 1;

		auto result = vkCreateFramebuffer(mainDevice.logicalDevice, &framebufferCreateInfo, nullptr, &swapChainFramebuffers[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create framebuffer!");
		}
	}
}

void VulkanRenderer::createCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = getQueueFamilies(mainDevice.physicalDevice);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;	// in vkBeginCmd it resets the command now which are created from this pool
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;	// queue family type that buffers from this command pool it will use

	// create a graphics queue family command pool
	auto result = vkCreateCommandPool(mainDevice.logicalDevice, &poolInfo, nullptr, &graphicsCommandPool);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create command pool!");
	}
}

void VulkanRenderer::createCommandBuffers()
{
	// resize commandbuffer to 1 per framebuffer
	commandBuffers.resize(swapChainFramebuffers.size());

	VkCommandBufferAllocateInfo cbAllocInfo{};
	cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.commandPool = graphicsCommandPool;
	cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;		//vk_command_buffer_level_primary you submit directly to the queue - secondary -> command_buffer which submit to a other command_buffer throw vkcmdexecutecommands from a primary buffer
	cbAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

	// allocate command buffers and places handles in arrays of buffers
	auto result = vkAllocateCommandBuffers(mainDevice.logicalDevice, &cbAllocInfo, commandBuffers.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocated command buffers!");
	}
}

void VulkanRenderer::createSynchronisation()
{
	imageAvailable.resize(MAX_FRAME_DRAWS);
	renderFinished.resize(MAX_FRAME_DRAWS);
	drawFences.resize(MAX_FRAME_DRAWS);

	// semaphore creation information
	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	// fence creation information
	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;			// fence starts signaled (open) so it doesn't block at start

	for (auto i = 0lu; i < MAX_FRAME_DRAWS; i++)
	{
		if (vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailable[i]) != VK_SUCCESS || 
			vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinished[i]) != VK_SUCCESS ||
			vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &drawFences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create semaphores / fences!");
		}
	}
}

void VulkanRenderer::createTextureSampler()
{
	//sampler creation info
	VkSamplerCreateInfo samplerCreateInfo{};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR; //how to render when image is magnified on screen
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR; //how to render when image is minified on screen
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;		//how to handle texture wrap in u (x) direction
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;		//how to handle texture wrap in u (y) direction
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;		//how to handle texture wrap in u (z) direction
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;		// if we set clamp to border - border beyond texture
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;					// whether coords should be normalized between 0 and 1
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;			// mipmap interpolation mode
	samplerCreateInfo.mipLodBias = 0.f;										// level of detail bias for mip map
	samplerCreateInfo.minLod = 0.f;											// min level of detail to pick mip level
	samplerCreateInfo.maxLod = 0.f;											// max level of detail to pick mip level
	samplerCreateInfo.anisotropyEnable = VK_TRUE;							// enable anisotropy
	samplerCreateInfo.maxAnisotropy = 16;									// x16 anisotropy

	auto result = vkCreateSampler(mainDevice.logicalDevice, &samplerCreateInfo, nullptr, &textureSampler);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create texture sampler");
	}
}

void VulkanRenderer::createUniformBuffers()
{
	// buffer size will be size of all 2 variables (will offset to access) - view projection size
	VkDeviceSize vpBufferSize = sizeof(UboViewProjection);

	//model buffer size
	//VkDeviceSize modelDynBufferSize = modelUniformAlignment * MAX_OBJECTS;


	//one uniform buffer for each image (and by extension, command buffer)
	vpUniformBuffers.resize(swapChainImages.size());
	vpUniformBufferMemory.resize(swapChainImages.size());
/*
	modelDynUniformBuffers.resize(swapChainImages.size());
	modelDynUniformBufferMemory.resize(swapChainImages.size());*/

	//create uniform buffers
	for (auto i = 0lu; i < swapChainImages.size(); i++)
	{
		createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, vpBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vpUniformBuffers[i], vpUniformBufferMemory[i]);
/*
		createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, modelDynBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, modelDynUniformBuffers[i], modelDynUniformBufferMemory[i]);*/
	}
}

void VulkanRenderer::createDescriptorPool()
{
	//create uniform descriptor pool

	// type of descriptors + how many descriptors, not descriptor sets (combined makes the poolsize)

	// View Projection Pool
	VkDescriptorPoolSize vpPoolSize{};
	vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffers.size());

	// Model pool (dynamic)
	/*VkDescriptorPoolSize modelPoolSize{};
	modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	modelPoolSize.descriptorCount = static_cast<uint32_t>(modelDynUniformBuffers.size());
	*/
	std::array<VkDescriptorPoolSize, 1> poolSizes = { vpPoolSize }; //, modelPoolSize

	// data to create descriptor pool
	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.maxSets = static_cast<uint32_t>(swapChainImages.size());		// maximum number of descriptor sets that can be created from pool
	poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());											// amount of pool sizes being passed
	poolCreateInfo.pPoolSizes = poolSizes.data();										// pool sizes to create pool with

	// create descriptor pool
	auto result = vkCreateDescriptorPool(mainDevice.logicalDevice, &poolCreateInfo, nullptr, &descriptorPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor pool!");
	}


	// create sampler descriptor pool
	//texture sampler pool

	VkDescriptorPoolSize samplerPoolSize{};
	samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; //sperate is probably more optimal TODO
	samplerPoolSize.descriptorCount = MAX_OBJECTS;					  // assuming we only have one texture per object - not optimal TODO

	VkDescriptorPoolCreateInfo samplerPoolCreateInfo{};
	samplerPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	samplerPoolCreateInfo.maxSets = MAX_OBJECTS;					// should use textureAtlas or arraylayers TODO
	samplerPoolCreateInfo.poolSizeCount = 1;
	samplerPoolCreateInfo.pPoolSizes = &samplerPoolSize;

	result = vkCreateDescriptorPool(mainDevice.logicalDevice, &samplerPoolCreateInfo, nullptr, &samplerDescriptorPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create sampler descriptor pool!");
	}
}

void VulkanRenderer::createDescriptorSets()
{
	// resize descriptor set list so we have one for every buffer
	descriptorSets.resize(swapChainImages.size());

	std::vector<VkDescriptorSetLayout> setLayouts(swapChainImages.size(), descriptorSetLayout);

	VkDescriptorSetAllocateInfo setAllocInfo{};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = descriptorPool;				// pool to allocate descriptor set from
	setAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());			// number of sets to allocate
	setAllocInfo.pSetLayouts = setLayouts.data();			// first descriptor set uses first element, second uses second element etc, all atm have descriptorSetLayout - layouts to use to allocate sets)

	// allocate descriptor sets
	auto result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, descriptorSets.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate descriptor sets!");
	}

	// update all of descriptor set buffer bindings
	for (auto i = 0lu; i < swapChainImages.size(); i++)
	{
		// buffer info and data offset info

		// view projection
		VkDescriptorBufferInfo vpBufferInfo{};
		vpBufferInfo.buffer = vpUniformBuffers[i];			// buffer to get data from
		vpBufferInfo.offset = 0;							// position of start of data
		vpBufferInfo.range = sizeof(UboViewProjection);					// size of data

		// data about connection between binding and buffer
		VkWriteDescriptorSet vpSetWrite{};
		vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		vpSetWrite.dstSet = descriptorSets[i];	// descriptor set to update
		vpSetWrite.dstBinding = 0;				// layout(binding = 0) uniform MVP this here - binding to update
		vpSetWrite.dstArrayElement = 0;		// index in the array we want to update
		vpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;		// type of descriptor
		vpSetWrite.descriptorCount = 1;		// amount to update
		vpSetWrite.pBufferInfo = &vpBufferInfo;	// information about buffer data to bind


		// model descriptor

		////model buffer binding
		//VkDescriptorBufferInfo modelBufferInfo{};
		//modelBufferInfo.buffer = modelDynUniformBuffers[i];		
		//modelBufferInfo.offset = 0;						
		//modelBufferInfo.range = modelUniformAlignment;		

		//// data about connection between binding and buffer
		//VkWriteDescriptorSet modelSetWrite{};
		//modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		//modelSetWrite.dstSet = descriptorSets[i];
		//modelSetWrite.dstBinding = 1;
		//modelSetWrite.dstArrayElement = 0;
		//modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		//modelSetWrite.descriptorCount = 1;
		//modelSetWrite.pBufferInfo = &modelBufferInfo;

		std::array<VkWriteDescriptorSet, 1> writeDescriptorSets{ vpSetWrite }; //, modelSetWrite };

		// update the descriptor sets with new buffer / binding info
		vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}

VkFormat VulkanRenderer::getDepthBufferFormat()
{
	//get supported format for depth buffer
	return chooseSupportedFormat({ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT } //stencil, normal depth, depth 24 normalised
	, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void VulkanRenderer::updateUniformBuffers(uint32_t imageIndex)
{
	void* data;

	// copy VP data
	vkMapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex], 0, sizeof(UboViewProjection), 0, &data);
	memcpy(data, &uboViewProjection, sizeof(UboViewProjection));
	vkUnmapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex]);


	// copy model data

	//// copy data to aligned offset modelTransferSpace
	//for (auto i = 0lu; i < meshList.size(); i++)
	//{
	//	Model* model = (Model*)((uint64_t)modelTransferSpace + (i * modelUniformAlignment));
	//	*model = meshList[i].getModel();
	//}

	////map the list of model data 
	//vkMapMemory(mainDevice.logicalDevice, modelDynUniformBufferMemory[imageIndex], 0, modelUniformAlignment * meshList.size() , 0, &data); // not max object, since we only want the amount of object that exist
	//memcpy(data, modelTransferSpace, modelUniformAlignment * meshList.size());
	//vkUnmapMemory(mainDevice.logicalDevice, modelDynUniformBufferMemory[imageIndex]);
}

void VulkanRenderer::recordCommands(uint32_t currentImage)
{
	// information about how to begin each command buffer
	VkCommandBufferBeginInfo bufferBeginInfo{};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	// not needed anymore due to fences
	//bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;		// buffer can be resubmitted when it has already been submitted and is awaiting execution

	// infromation about how to begin render pass only needed for graphical applications
	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.renderArea.offset = { 0, 0 };		// start point of render pass in pixels
	renderPassBeginInfo.renderArea.extent = swapChainExtent; //size of region to run renderpass on, starting at offset

	std::array<VkClearValue, 2> clearValues;
	clearValues[0].color = { 0.6f, 0.65f, 0.4f, 1.f };
	clearValues[1].depthStencil.depth = 1.f;

	renderPassBeginInfo.pClearValues = clearValues.data();
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());;


	renderPassBeginInfo.framebuffer = swapChainFramebuffers[currentImage];

	// start recording commands to commandbuffer
	auto result = vkBeginCommandBuffer(commandBuffers[currentImage], &bufferBeginInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to start recording a commandbuffer!");
	}

	{
		// begin render pass
		vkCmdBeginRenderPass(commandBuffers[currentImage], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);  //all of the commands will be primary commands

		{
			//bind pipeline to be used in render pas
			vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

			for (auto k = 0lu; k < meshList.size(); k++)
			{
				auto& mesh = meshList[k];

				VkBuffer vertexBuffers[] = { mesh.getVertexBuffer() };			// buffers to bind
				VkDeviceSize offsets[] = { 0 };										// offsets into buffers being bound
				vkCmdBindVertexBuffers(commandBuffers[currentImage], 0, 1, vertexBuffers, offsets);  // command to bind vertex buffer before with them

				// bind mesh index buffer, with 0 offset and uisng uint32
				vkCmdBindIndexBuffer(commandBuffers[currentImage], mesh.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);


				// dynamic offset amount
				//uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignment) * k;

				//push constants to shader stage directly
				vkCmdPushConstants(commandBuffers[currentImage], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Model), &meshList[k].getModel());

				// bind descriptor sets
				
				std::array<VkDescriptorSet, 2> descriptorSetGroup{ descriptorSets[currentImage], samplerDescriptorSets[meshList[k].getTexId()] };
				
				//vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentImage],
				//	1, &dynamicOffset); //1 dynamic offset for dynamic uniformbuffer
				vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, static_cast<uint32_t>(descriptorSetGroup.size())
					, descriptorSetGroup.data(), 
					0, nullptr); //1 dynamic offset for dynamic uniformbuffer

				//execute pipeline
				vkCmdDrawIndexed(commandBuffers[currentImage], mesh.getIndexCount(), 1, 0, 0, 0);
			}

		}

		// end renderpass
		vkCmdEndRenderPass(commandBuffers[currentImage]);
	}

	// stop recording to command 
	result = vkEndCommandBuffer(commandBuffers[currentImage]);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to stop recording a commandbuffer");
	}

	//vkBeginCommandBuffer(comm)
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

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &deviceProperties);

	//minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;

}

//void VulkanRenderer::allocateDynamicBufferTransferSpace()
//{
//	// calculate alignment of model data
//	modelUniformAlignment = (sizeof(Model) + minUniformBufferOffset-1) & ~(minUniformBufferOffset - 1);
//
//	// cteated space in memory to hold dynamic buffer that is aligned to our required alignment and holds max_objects
//	modelTransferSpace = (Model*)_aligned_malloc(modelUniformAlignment * MAX_OBJECTS, modelUniformAlignment);
//}

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

	////information about what the device can do (geo shader , tessellation shader, wide lines, etc)

	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	auto extensionSupported = checkDeviceExtensionSupport(device);
	auto swapChainValid = false;
	
	if (extensionSupported)
	{
		SwapChainDetails swapChainDetails = getSwapChainDetails(device);
		swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
	}

	QueueFamilyIndices indices = getQueueFamilies(device);
	return indices.isValid() && extensionSupported  && swapChainValid && deviceFeatures.samplerAnisotropy;
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

VkFormat VulkanRenderer::chooseSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags)
{
	// loop through the options and find compatible one

	for (auto& format : formats)
	{
		// get properties for given format on this device
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(mainDevice.physicalDevice, format, &properties);

		//depending on tiling choice, need to check for different bit flag
		if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
	}

	throw std::runtime_error("Failed to find a matching format!");
}

VkImage VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags useFlags, VkMemoryPropertyFlags propFlags, VkDeviceMemory& imageMemory)
{
	// create image
	VkImageCreateInfo imageCreateInfo{};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;		// 1d , 2d, 3d
	imageCreateInfo.extent.width = width;
	imageCreateInfo.extent.height = height;
	imageCreateInfo.extent.depth = 1;			// depth of image extende (just 1, no 3d aspect)
	imageCreateInfo.mipLevels = 1;				//number of mipmaplevels 
	imageCreateInfo.arrayLayers = 1;			// number of leves in image array - cubemaps
	imageCreateInfo.format = format;			// format type of image
	imageCreateInfo.tiling = tiling;			// how image data should br tiled (arranged for optimal usage)
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // layout of image data on creation
	imageCreateInfo.usage = useFlags;			// bit flags defining what image will be used for
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT; // number of samples for multi sampling
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Whether image can be shared between queues

	VkImage image;
	auto result = vkCreateImage(mainDevice.logicalDevice, &imageCreateInfo, nullptr, &image);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an image!");
	}

	// create memory for image

	// get memory requirements for a type of image
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(mainDevice.logicalDevice, image, &memoryRequirements);


	// allocate memory using image requirements and user defined properties
	VkMemoryAllocateInfo memoryAllocInfo{};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memoryRequirements.size;
	memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(mainDevice.physicalDevice, memoryRequirements.memoryTypeBits, propFlags);

	result = vkAllocateMemory(mainDevice.logicalDevice, &memoryAllocInfo, nullptr, &imageMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate memory for image!");
	}

	// connect memory to image
	vkBindImageMemory(mainDevice.logicalDevice, image, imageMemory, 0);

	return image;
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

int VulkanRenderer::createTextureImage(const std::string& filename)
{
	int width, height;
	VkDeviceSize imageSize;
	stbi_uc* imageData = loadTextureFile(filename, width, height, imageSize);


	//create staging buffer to hold loaded data, ready to copy to device
	VkBuffer imageStagingBuffer;
	VkDeviceMemory imageStagingBufferMemory;
	createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, imageStagingBuffer, imageStagingBufferMemory);

	void *data;
	vkMapMemory(mainDevice.logicalDevice, imageStagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, imageData, static_cast<size_t>(imageSize));
	vkUnmapMemory(mainDevice.logicalDevice, imageStagingBufferMemory);

	//Free original image data
	stbi_image_free(imageData);


	// create image to hold final data
	VkImage texImage;
	VkDeviceMemory texImageMemory;

	texImage = createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texImageMemory);
	
	// copy data to image
	
	// transition image to be dst for copy operation
	transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, texImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// copy image data
	copyImageBuffer(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, imageStagingBuffer, texImage, width, height);

	//transition image to be shader readable for shader usage
	transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, texImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// add texture data to vector for reference
	textureImages.push_back(texImage);
	textureImageMemory.push_back(texImageMemory);

	// destroy stating buffers
	vkDestroyBuffer(mainDevice.logicalDevice, imageStagingBuffer, nullptr);
	vkFreeMemory(mainDevice.logicalDevice, imageStagingBufferMemory, nullptr);

	return static_cast<int>(textureImages.size()) - 1;
}

int VulkanRenderer::createTexture(const std::string& filename)
{
	//create texture image and get its location in the array
	auto textureImageLoc = createTextureImage(filename);

	// create image view and add to list
	VkImageView imageView = createImageView(textureImages[textureImageLoc], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	textureImageViews.push_back(imageView);

	//create descriptor set
	auto descriptorLoc = createTextureDescriptor(imageView);

	//return location of set with texture
	return descriptorLoc;
}

int VulkanRenderer::createTextureDescriptor(VkImageView textureImage)
{
	
	// descriptorset allocation info
	VkDescriptorSetAllocateInfo setAllocInfo{};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = samplerDescriptorPool;
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &samplerSetLayout;

	VkDescriptorSet descriptorSet;

	//allocate descriptor sets
	auto result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, &descriptorSet);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate texture descriptor sets!");
	}

	//texture image info
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // image layout when in use
	imageInfo.imageView = textureImage;			// image to bind to set
	imageInfo.sampler = textureSampler;			// sampler to bind to set

	// descriptor write info
	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = descriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;

	// update new descriptor set
	vkUpdateDescriptorSets(mainDevice.logicalDevice, 1, &descriptorWrite, 0, nullptr);

	// add descriptor set to list
	samplerDescriptorSets.push_back(descriptorSet);

	// return descriptor set location
	return samplerDescriptorSets.size() - 1;
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

stbi_uc* VulkanRenderer::loadTextureFile(const std::string& filename, int& width, int& height, VkDeviceSize& imageSize)
{
	// number of channels image uses
	int channels;

	//load pixel data for image
	std::string fileloc = "C:/Projects/Vulkan/VulkanTest/Textures/" + filename;
	stbi_uc* image = stbi_load(fileloc.c_str(), &width, &height, &channels, STBI_rgb_alpha);

	if (!image)
	{
		throw std::runtime_error("Failed to load texture file " + filename);
	}

	//calculate image size using given and known data
	constexpr static const auto channelSize = 4;
	imageSize = width * height * channelSize;

	return image;
}
