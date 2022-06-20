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
		createRenderPass();
		createDescriptorSetLayout();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();

		mvp.projection = glm::perspective(glm::radians(45.f), (float)swapChainExtent.width / (float)swapChainExtent.height, 0.f, 100.f);
		mvp.view = glm::lookAt(glm::vec3(0.f, 0.f, 2.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f));
		mvp.model = glm::mat4(1.f);

		mvp.projection[1][1] *= -1;

		// Create a mesh

		//vertex data
		std::vector<Vertex> meshVertices =
		{
			{{-0.1, -0.4, 0.0}, {1.f, 0.f, 0.f}}, //0
			{{-0.1, 0.4, 0.0}, {0.f, 1.f, 0.f}} , //1
			{{-0.9, 0.4, 0.0}, {0.f, 0.f, 1.f}}, //2
			{{-0.9, -0.4, 0.0}, {0.f, 1.f, 0.f}} , //3
		};

		std::vector<Vertex> meshVertices2 =
		{
			{{0.9, -0.4, 0.0}, {1.f, 0.f, 0.f}}, //0
			{{0.9, 0.4, 0.0}, {0.f, 1.f, 0.f}} , //1
			{{0.1, 0.4, 0.0}, {0.f, 0.f, 1.f}}, //2
			{{0.1, -0.4, 0.0}, {0.f, 1.f, 0.f}} , //3
		};

		// index data
		std::vector<uint32_t> meshIndices = {
			0, 1, 2,
			2 ,3, 0
		};

		Mesh firstMesh = Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, meshVertices, meshIndices);

		Mesh secondMesh = Mesh(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, meshVertices2, meshIndices);
		meshList.push_back(firstMesh);
		meshList.push_back(secondMesh);

		createCommandBuffers();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		recordCommands();
		createSynchronisation();
	}
	catch (const std::runtime_error& e)
	{
		printf("ERROR: %s\n", e.what());
		return EXIT_FAILURE;
	}

	return 0;
}

void VulkanRenderer::updateModel(glm::mat4 newModel)
{
	mvp.model = newModel;
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

	updateUniformBuffer(imageIndex);

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

	vkDestroyDescriptorPool(mainDevice.logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descriptorSetLayout, nullptr);

	for (auto i = 0lu; i < uniformBuffers.size(); i++)
	{
		vkDestroyBuffer(mainDevice.logicalDevice, uniformBuffers[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, uniformBufferMemory[i], nullptr);
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

void VulkanRenderer::createDescriptorSetLayout()
{
	// mvp binding info
	VkDescriptorSetLayoutBinding mvpLayoutBinding{};
	mvpLayoutBinding.binding = 0;				// layout(binding=0) in vert shader |binding point in sharder designated by bdining number in shader)
	mvpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // type of descriptor (uniform , dynamic uniform, image sampler, etc)
	mvpLayoutBinding.descriptorCount = 1;					// number of descriptors for binding (atm only MVP)
	mvpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;	//shader stage to bind to (vert in our case)
	mvpLayoutBinding.pImmutableSamplers = nullptr;			// for texture: can make sampler unchangeable (immutable) by specifying layout


	// create descriptor set layout with given bindings
	VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.bindingCount = 1;					// number of binding infos
	layoutCreateInfo.pBindings = &mvpLayoutBinding;		// array of  binding infos

	//create descriptor set layout
	auto result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Faild to create descriptor set layout!");
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
	
	// how the data for a single vertex (including info such as pos, color texcoords, normals, etc) is as a whole
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;					// can bind multiple streams of data, this defines which one
	bindingDescription.stride = sizeof(Vertex);		// size of a single vertex object
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;	// how to move between data after each vertex
																// vk_vertex_inuput_rate_vertex : move on to the next vertex
																// vk_vertex_input_rate_instance : move to a vertex for the next instance

	// how the data for an attribute is defined within a vertex
	std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions;

	//Position attribute
	attributeDescriptions[0].binding = 0; // layout(binding=0, location=0) int vec3 pos; its the invisible binding feature | which binding the data is at (should be same as above unles you have more streams)
	attributeDescriptions[0].location = 0; // location in the above mentioned location feature of the shader - location in shader where data will be read from
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT; // format the data will take (also helps define the size of the data)
	attributeDescriptions[0].offset = offsetof(Vertex, pos);		// where this attribute is defined in the data for a signle vertex

	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(Vertex, col);

	// color attribute


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
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
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

void VulkanRenderer::createFramebuffers()
{
	// resize to the amount of swapchainimages , we create 1 framebuffer per image
	swapChainFramebuffers.resize(swapChainImages.size());

	//create a framebuffer for each swap chain image
	for (auto i = 0lu; i < swapChainFramebuffers.size(); i++)
	{
		std::array<VkImageView, 1> attachments = { swapChainImages[i].imageView };

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

void VulkanRenderer::createUniformBuffers()
{
	// buffer size will be size of all 3 variables (will offset to access)
	VkDeviceSize bufferSize = sizeof(MVP);

	//one uniform buffer for each image (and by extension, command buffer)
	uniformBuffers.resize(swapChainImages.size());
	uniformBufferMemory.resize(swapChainImages.size());

	//create uniform buffers
	for (auto i = 0lu; i < swapChainImages.size(); i++)
	{
		createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBufferMemory[i]);
	}
}

void VulkanRenderer::createDescriptorPool()
{
	// type of descriptors + how many descriptors, not descriptor sets (combined makes the poolsize)
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = static_cast<uint32_t>(uniformBuffers.size());


	// data to create descriptor pool
	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.maxSets = static_cast<uint32_t>(uniformBuffers.size());		// maximum number of descriptor sets that can be created from pool
	poolCreateInfo.poolSizeCount = 1;											// amount of pool sizes being passed
	poolCreateInfo.pPoolSizes = &poolSize;										// pool sizes to create pool with

	// create descriptor pool
	auto result = vkCreateDescriptorPool(mainDevice.logicalDevice, &poolCreateInfo, nullptr, &descriptorPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create descriptor pool!");
	}
}

void VulkanRenderer::createDescriptorSets()
{
	// resize descriptor set list so we have one for every buffer
	descriptorSets.resize(uniformBuffers.size());

	std::vector<VkDescriptorSetLayout> setLayouts(uniformBuffers.size(), descriptorSetLayout);

	VkDescriptorSetAllocateInfo setAllocInfo{};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = descriptorPool;				// pool to allocate descriptor set from
	setAllocInfo.descriptorSetCount = static_cast<uint32_t>(uniformBuffers.size());			// number of sets to allocate
	setAllocInfo.pSetLayouts = setLayouts.data();			// first descriptor set uses first element, second uses second element etc, all atm have descriptorSetLayout - layouts to use to allocate sets)

	// allocate descriptor sets
	auto result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, descriptorSets.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate descriptor sets!");
	}

	// update all of descriptor set buffer bindings
	for (auto i = 0lu; i < uniformBuffers.size(); i++)
	{
		// buffer info and data offset info
		VkDescriptorBufferInfo mvpBufferInfo{};
		mvpBufferInfo.buffer = uniformBuffers[i];			// buffer to get data from
		mvpBufferInfo.offset = 0;							// position of start of data
		mvpBufferInfo.range = sizeof(MVP);					// size of data

		// data about connection between binding and buffer
		VkWriteDescriptorSet mvpSetWrite{};
		mvpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		mvpSetWrite.dstSet = descriptorSets[i];	// descriptor set to update
		mvpSetWrite.dstBinding = 0;				// layout(binding = 0) uniform MVP this here - binding to update
		mvpSetWrite.dstArrayElement = 0;		// index in the array we want to update
		mvpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;		// type of descriptor
		mvpSetWrite.descriptorCount = 1;		// amount to update
		mvpSetWrite.pBufferInfo = &mvpBufferInfo;	// information about buffer data to bind

		// update the descriptor sets with new buffer / binding info
		vkUpdateDescriptorSets(mainDevice.logicalDevice, 1, &mvpSetWrite, 0, nullptr);
	}
}

void VulkanRenderer::updateUniformBuffer(uint32_t imageIndex)
{
	void* data;
	vkMapMemory(mainDevice.logicalDevice, uniformBufferMemory[imageIndex], 0, sizeof(MVP), 0, &data);
	memcpy(data, &mvp, sizeof(MVP));
	vkUnmapMemory(mainDevice.logicalDevice, uniformBufferMemory[imageIndex]);
}

void VulkanRenderer::recordCommands()
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
	
	VkClearValue clearValues[] = { {0.6f, 0.65f, 0.4f, 1.f} };		// list of clear values TODO: depth attachment clear value
	renderPassBeginInfo.pClearValues = clearValues;
	
	renderPassBeginInfo.clearValueCount = 1;

	for (auto i = 0lu; i < commandBuffers.size(); i++)
	{
		renderPassBeginInfo.framebuffer = swapChainFramebuffers[i];

		// start recording commands to commandbuffer
		auto result = vkBeginCommandBuffer(commandBuffers[i], &bufferBeginInfo);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to start recording a commandbuffer!");
		}

		{
			// begin render pass
			vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);  //all of the commands will be primary commands

			{
				//bind pipeline to be used in render pas
				vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

				for (auto& mesh : meshList)
				{
					VkBuffer vertexBuffers[] = { mesh.getVertexBuffer() };			// buffers to bind
					VkDeviceSize offsets[] = { 0 };										// offsets into buffers being bound
					vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);  // command to bind vertex buffer before with them

					// bind mesh index buffer, with 0 offset and uisng uint32
					vkCmdBindIndexBuffer(commandBuffers[i], mesh.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

					// bind descriptor sets
					vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i],
						0, nullptr);

					//execute pipeline
					vkCmdDrawIndexed(commandBuffers[i], mesh.getIndexCount(), 1, 0, 0, 0);
				}

			}

			// end renderpass
			vkCmdEndRenderPass(commandBuffers[i]);
		}


		// stop recording to command 
		result = vkEndCommandBuffer(commandBuffers[i]);
		if (result != VK_SUCCESS)
		{
			std::runtime_error("failed to stop recording a commandbuffer");
		}
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

	////information about what the device can do (geo shader , tessellation shader, wide lines, etc)
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
