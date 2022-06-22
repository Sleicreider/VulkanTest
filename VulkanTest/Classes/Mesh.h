#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

#include "Utilities.h"

struct Model
{
	glm::mat4 model;
};

class Mesh
{
public:
	Mesh();
	Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);

	void setModel(glm::mat4 newModel);
	Model getModel() const;

	int getVertexCount() const;
	int getIndexCount() const;

	VkBuffer getVertexBuffer() const;
	VkBuffer getIndexBuffer() const;

	void destroyBuffers();

private:
	void createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex>& vertices);
	void createIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<uint32_t>& indices);
	uint32_t findMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags properties);

private:
	Model model;

	int vertexCount;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;

	int indexCount;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

	VkPhysicalDevice physicalDevice;
	VkDevice device;
};