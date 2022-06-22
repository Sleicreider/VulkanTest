#include <iostream>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE

//auto include vulkan
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include "VulkanRenderer.h"

#include <stdexcept>
#include <vector>

GLFWwindow* window = nullptr;
VulkanRenderer vulkanRenderer;

void initWindow(const std::string& wName = "Test Window", const int width = 800, const int height = 600)
{
	//initialize glfw
	glfwInit();

	//set glfw to not work with OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);
}

int main()
{
	initWindow("Test Window", 800, 600);

	// create vulkan renderer instance
	if (vulkanRenderer.init(window) == EXIT_FAILURE)
	{
		return EXIT_FAILURE;
	}

	float angle = 0.f;
	float deltaTime = 0.f;
	float lastTime = 0.f;

	//loop until close
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		float now = glfwGetTime();
		deltaTime = now - lastTime;
		lastTime = now;


		angle += 10.f * deltaTime;
		if (angle > 360.f)
		{
			angle -= 360.f;
		}

		glm::mat4 firstModel(1.f);
		glm::mat4 secondModel(1.f);

		//firstModel = glm::translate(firstModel, glm::vec3(-2.f, 0.f, -5.f));
		firstModel = glm::rotate(firstModel, glm::radians(angle), glm::vec3(0.f, 0.f, 1.f));

		//secondModel = glm::translate(secondModel, glm::vec3(2.f, 0.f, -5.f));
		secondModel = glm::rotate(secondModel, glm::radians(-angle * 100.f), glm::vec3(0.f, 0.f, 1.f));

		vulkanRenderer.updateModel(0, firstModel);
		vulkanRenderer.updateModel(1, secondModel);

		vulkanRenderer.draw();
	}

	vulkanRenderer.cleanup();

	//destroy glfw window and stop glfw
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}