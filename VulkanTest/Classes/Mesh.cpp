#include "Mesh.h"

Mesh::Mesh()
{
}

Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, std::vector<Vertex>& vertices)
{
	vertexCount = vertices.size();
	physicalDevice = newPhysicalDevice;
	device = newDevice;
	vertexBuffer = createVertexBuffer(vertices);
}

int Mesh::getVertexCount() const
{
	return vertexCount;
}

VkBuffer Mesh::getVertexBuffer() const
{
	return vertexBuffer;
}

void Mesh::destroyVertexBuffer()
{
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
}

VkBuffer Mesh::createVertexBuffer(std::vector<Vertex>& vertices)
{
	// CREATE VERTEX BUFFER

	// info to create a buffer (doesnt include assigning memory)
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(Vertex) * vertices.size();			// size of the buffer (size of 1 vertex * number of vertices)
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;		// multiple types of buffer possible, we want a Vertex buffer
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;			// similar to swap chain images, can share vertex buffers

	auto result = vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Vertex buffer");
	}

	//get buffer memory requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);

	//allocate memory to buffer
	VkMemoryAllocateInfo memAllocInfo{};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = memRequirements.size;
	memAllocInfo.memoryTypeIndex = findMemoryTypeIndex(memRequirements.memoryTypeBits,	//inde of memory type on physical device that has required bitflags
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		// VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : cpu can interact with memory
		// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : allows placement of data straight into buffer after mapping (otherwise have to specify manually)

	// allocate memory to VkDeviceMemory
	result = vkAllocateMemory(device, &memAllocInfo, nullptr, &vertexBufferMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocated Vertex Buffer Memory!");
	}

	// allocate memory to given vertex buffer
	vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);  // offset in memory is 0, we dont have anything else the using the memory where we would an offset for

	// MAP MEMORY TO VERTEX BUFFER
	void* data;																	// 1. create pointer to a point in normal memory
	vkMapMemory(device, vertexBufferMemory, 0, bufferInfo.size, 0, &data);		//2. map the vertex buffer memory to that point
	memcpy(data, vertices.data(), (size_t)bufferInfo.size);						//3. copy memory from certices vector to the point
	vkUnmapMemory(device, vertexBufferMemory);									//4. unmap the vertex memory
}

uint32_t Mesh::findMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags properties)
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
