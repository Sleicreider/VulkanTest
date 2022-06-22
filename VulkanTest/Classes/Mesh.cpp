#include "Mesh.h"

Mesh::Mesh()
{
}

Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
{
	vertexCount = vertices.size();
	indexCount = indices.size();
	physicalDevice = newPhysicalDevice;
	device = newDevice;
	createVertexBuffer(transferQueue, transferCommandPool, vertices);
	createIndexBuffer(transferQueue, transferCommandPool, indices);

	model.model = glm::mat4(1.f);
}

void Mesh::setModel(glm::mat4 newModel)
{
	model.model = newModel;
}

Model Mesh::getModel() const
{
	return model;
}

int Mesh::getVertexCount() const
{
	return vertexCount;
}

int Mesh::getIndexCount() const
{
	return indexCount;
}

VkBuffer Mesh::getVertexBuffer() const
{
	return vertexBuffer;
}

VkBuffer Mesh::getIndexBuffer() const
{
	return indexBuffer;
}

void Mesh::destroyBuffers()
{
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);

	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, indexBufferMemory, nullptr);
}

void Mesh::createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex>& vertices)
{
	// Get size of buffer needed of vertices
	VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();

	//Tmp buffer to "stage" vertex data before transferring to GPU

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	// create staging buffer and allocate memory to it
	createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory);

	// MAP MEMORY TO VERTEX BUFFER
	void* data;																	// 1. create pointer to a point in normal memory
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);		//2. map the vertex buffer memory to that point
	memcpy(data, vertices.data(), (size_t)bufferSize);						//3. copy memory from certices vector to the point
	vkUnmapMemory(device, stagingBufferMemory);									//4. unmap the vertex memory

	// create buffer with transfer_dst_bit to mark as recipient of transfer data (also vertex_buffer)
	// buffer memory is to be DEVICE_LOCAL_BIT meaning memory is on the gpu and only accessible by it and not cpu(host)
	createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

	// copy staging buffer to vertex buffer on GPU
	copyBuffer(device, transferQueue, transferCommandPool, stagingBuffer, vertexBuffer, bufferSize);

	//cleanup staging buffer parts
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Mesh::createIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<uint32_t>& indices)
{
	// Get size of buffer needed of indices
	VkDeviceSize bufferSize = sizeof(uint32_t) * indices.size();

	//Tmp buffer to "stage" vertex data before transferring to GPU

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	// MAP MEMORY TO INDEX BUFFER
	void* data;																	
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);		
	memcpy(data, indices.data(), (size_t)bufferSize);						
	vkUnmapMemory(device, stagingBufferMemory);									

	// create buffer for index data on gpu access only area
	createBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);


	// copy staging buffer to gpu access buffer
	copyBuffer(device, transferQueue, transferCommandPool, stagingBuffer, indexBuffer, bufferSize);

	// destroy + release staging buffer and memory
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}
