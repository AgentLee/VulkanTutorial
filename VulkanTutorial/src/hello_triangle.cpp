#include "hello_triangle.h"
#include "vulkan_helper.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

//#define STB_IMAGE_IMPLEMENTATION
//#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <map>
#include <set>
#include <cstdint>
#include <algorithm>
#include <unordered_map>


//#include "texture.h"
#include "../libs/imgui/imgui_impl_glfw.h"
#include "../libs/imgui/imgui_impl_vulkan.h"

#define IMGUI_ENABLED true

void HelloTriangle::Run()
{
	InitWindow();

	VulkanManager::CreateVulkanManager(m_window);
	VulkanManager::GetVulkanManager().Initialize();
	
	InitVulkan();
	
#if IMGUI_ENABLED
	InitImGui();
#endif

	MainLoop();

	Cleanup();
}

void HelloTriangle::MainLoop()
{
	while (!glfwWindowShouldClose(m_window))
	{
		glfwPollEvents();
		DrawFrame();
	}

	vkDeviceWaitIdle(VulkanManager::GetVulkanManager().GetDevice());
}

void HelloTriangle::DrawFrame()
{	
	// -Get image from swap chain
	// -Execute command buffer with the image as an attachment in the frame buffer
	// -Return the image to the swap chain to present

	// These events are asynchronous, we need to sync them up.
	// Semaphores vs fences:
	// -Fences used for syncing rendering
	// -Semaphores used for syncing operations across command queues

	vkWaitForFences(VulkanManager::GetVulkanManager().GetDevice(), 1, &VulkanManager::GetVulkanManager().GetInFlightFences()[m_currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;	// Store index of the image from the swap chain.
	auto acquireResult = vkAcquireNextImageKHR(VulkanManager::GetVulkanManager().GetDevice(), VulkanManager::GetVulkanManager().GetSwapChain(), UINT64_MAX, VulkanManager::GetVulkanManager().GetImageAvailableSemaphores()[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

	// If swap chain is out of date or suboptimal, recreate it.
	if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR)
	{
		frameBufferResized = false;
		RecreateSwapChain();

#if IMGUI_ENABLED
		// Should be more elegant than this
		InitImGui();
#endif
		
		return;
	}
	else if (acquireResult != VK_SUCCESS)
	{
		ASSERT(false, "Failed to acquire swap chain image");
	}

	// Check if previous frame is using this image (waiting on a fence)
	if (VulkanManager::GetVulkanManager().GetImagesInFlight()[imageIndex] != VK_NULL_HANDLE)
	{
		vkWaitForFences(VulkanManager::GetVulkanManager().GetDevice(), 1, &VulkanManager::GetVulkanManager().GetImagesInFlight()[imageIndex], VK_TRUE, UINT64_MAX);
	}

	// Mark image as being used
	VulkanManager::GetVulkanManager().GetImagesInFlight()[imageIndex] = VulkanManager::GetVulkanManager().GetInFlightFences()[m_currentFrame];

	VkSemaphore waitSemaphores[] = { VulkanManager::GetVulkanManager().GetImageAvailableSemaphores()[m_currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSemaphore signalSemaphores[] = { VulkanManager::GetVulkanManager().GetRenderFinishedSemaphores()[m_currentFrame] };

	// Update uniforms
	m_sampleModel.SubmitDrawCall(imageIndex);

#if IMGUI_ENABLED
	// Submit ImGui commands
	m_imguiManager.SubmitDrawCall(imageIndex);
#endif

#if IMGUI_ENABLED
	std::array<VkCommandBuffer, 2> submitCommandBuffers =
	{
		m_commandBuffers[imageIndex],
		m_imguiManager.GetCommandBuffers()[imageIndex]
	};
#else
	std::array<VkCommandBuffer, 1> submitCommandBuffers =
	{
		m_commandBuffers[imageIndex],
	};
#endif
	
	// Submit command buffer
	VkSubmitInfo submitInfo{};
	{
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		// Specify which semaphores to wait on before execution and which stage to wait on.
		// We want to wait until the image is available so when the graphics pipeline writes to the color attachment.
		// The wait semaphores should correspond to the wait stages array.
		{
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.pWaitDstStageMask = waitStages;
		}

		// Specify which command buffers to submit for execution.
		// We want to submit the buffer that binds the swap chain image we acquired as color attachment.
		{
			submitInfo.commandBufferCount = submitCommandBuffers.size();
			submitInfo.pCommandBuffers = submitCommandBuffers.data();
		}

		// Specify which semaphores to signal after execution.
		{
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = signalSemaphores;
		}

		vkResetFences(VulkanManager::GetVulkanManager().GetDevice(), 1, &VulkanManager::GetVulkanManager().GetImagesInFlight()[m_currentFrame]);

		VK_ASSERT(vkQueueSubmit(VulkanManager::GetVulkanManager().GetGraphicsQueue(), 1, &submitInfo, VulkanManager::GetVulkanManager().GetImagesInFlight()[m_currentFrame]), "Failed to submit draw command buffer");
	}
	
	// Present
	// Submit result back to swap chain toc show on screen.

	VkSwapchainKHR swapChains[] = { VulkanManager::GetVulkanManager().GetSwapChain() };
	VkPresentInfoKHR presentInfo{};
	{
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		// Specify which semaphores to wait on before presenting.
		{
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = signalSemaphores;
		}

		// Specify swap chains to present images to and which image to present.
		{
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = swapChains;
			presentInfo.pImageIndices = &imageIndex;
		}

		// Specify array of results to check if the result was successful.
		presentInfo.pResults = nullptr;
	}

	vkQueuePresentKHR(VulkanManager::GetVulkanManager().GetPresentQueue(), &presentInfo);

	// Wait for work to finish after submission.
	vkQueueWaitIdle(VulkanManager::GetVulkanManager().GetPresentQueue());

	m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void HelloTriangle::FrameBufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto app = reinterpret_cast<HelloTriangle*>(glfwGetWindowUserPointer(window));
	app->frameBufferResized = true;
}

void HelloTriangle::InitWindow()
{
	// Initialize GLFW library
	glfwInit();

	// Disable OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// Create window
	// A cool thing here is that you can specify which monitor to open the window on.
	m_window = glfwCreateWindow(WIDTH, HEIGHT, "Hello Triangle", nullptr, nullptr);

	// Set pointer to window
	glfwSetWindowUserPointer(m_window, this);
	
	// Detect resizing
	glfwSetFramebufferSizeCallback(m_window, FrameBufferResizeCallback);
}

void HelloTriangle::InitImGui()
{
	// GLFW has to be initialized in main program?
	
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForVulkan(m_window, true);

	auto queueFamily = VulkanManager::GetVulkanManager().FindQueueFamilies(VulkanManager::GetVulkanManager().GetPhysicalDevice());
	auto swapChainSupport = VulkanManager::GetVulkanManager().QuerySwapChainSupport(VulkanManager::GetVulkanManager().GetPhysicalDevice());
	
	ImGui_ImplVulkan_InitInfo init_info = {};
	{
		init_info.Instance = VulkanManager::GetVulkanManager().GetInstance();
		init_info.PhysicalDevice = VulkanManager::GetVulkanManager().GetPhysicalDevice();
		init_info.Device = VulkanManager::GetVulkanManager().GetDevice();
		init_info.QueueFamily = queueFamily.graphicsFamily.value();
		init_info.Queue = VulkanManager::GetVulkanManager().GetGraphicsQueue();
		init_info.PipelineCache = nullptr;
		init_info.DescriptorPool = m_imguiManager.GetDescriptorPool();
		init_info.Allocator = nullptr;
		init_info.MinImageCount = swapChainSupport.capabilities.minImageCount + 1;
		init_info.ImageCount = VulkanManager::GetVulkanManager().NumSwapChainImages();
		init_info.CheckVkResultFn = nullptr;
	}
	ImGui_ImplVulkan_Init(&init_info, m_imguiManager.GetRenderPass());

	// Upload Fonts
	{
		// Use any command queue
		VkCommandPool command_pool = m_imguiManager.GetCommandPool();
		VK_ASSERT(vkResetCommandPool(VulkanManager::GetVulkanManager().GetDevice(), command_pool, 0), "");
		
		VkCommandBuffer command_buffer = m_imguiManager.GetCommandBuffers()[m_currentFrame];
		
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(command_buffer, &begin_info);

		ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

		VkSubmitInfo end_info = {};
		end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		end_info.commandBufferCount = 1;
		end_info.pCommandBuffers = &command_buffer;
		vkEndCommandBuffer(command_buffer);
		vkQueueSubmit(VulkanManager::GetVulkanManager().GetGraphicsQueue(), 1, &end_info, VK_NULL_HANDLE);

		vkDeviceWaitIdle(VulkanManager::GetVulkanManager().GetDevice());
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}

	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void HelloTriangle::InitVulkan()
{
	CreateRenderPasses();

	CreateDescriptorSetLayout();

	// Move to vulkan manager
	CreateGraphicsPipeline();

	CreateCommandPool();
	CreateColorResources();
	CreateDepthResources();
	CreateFrameBuffers();	// Since we have depth buffer, we need to make sure that's created first.

	m_sampleModel.Initialize();

	LoadModel();
	CreateVertexBuffer();
	CreateIndexBuffer();
	CreateUniformBuffers();
	CreateDescriptorPools();
	CreateDescriptorSets();
	CreateCommandBuffers();
}

void HelloTriangle::RecreateSwapChain()
{
	// Pause while the window is minimized.
	{
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(m_window, &width, &height);

		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(m_window, &width, &height);
			glfwWaitEvents();
		}
	}
	
	vkDeviceWaitIdle(VulkanManager::GetVulkanManager().GetDevice());

	CleanupSwapChain();
	
	VulkanManager::GetVulkanManager().CreateSwapChain();
	VulkanManager::GetVulkanManager().CreateImageViews();
	CreateRenderPasses();
	CreateGraphicsPipeline();
	CreateColorResources();
	CreateDepthResources();
	CreateFrameBuffers();
	CreateUniformBuffers();
	CreateDescriptorPools();
	CreateDescriptorSets();
	CreateCommandBuffers();
}

void HelloTriangle::CreateDescriptorSetLayout()
{
	m_sampleModel.CreateDescriptorSetLayout();
}

void HelloTriangle::CreateGraphicsPipeline()
{
	m_sampleModel.CreateGraphicsPipeline();
}

void HelloTriangle::CreateRenderPasses()
{
	m_sampleModel.CreateRenderPass();

#if IMGUI_ENABLED
	m_imguiManager.CreateRenderPass();
#endif
}

void HelloTriangle::CreateFrameBuffers()
{
	// Each FBO will need to reference each of the image view objects.

	m_swapChainFrameBuffers.resize(VulkanManager::GetVulkanManager().GetSwapChainImageViews().size());
	m_imguiManager.GetFrameBuffers().resize(VulkanManager::GetVulkanManager().GetSwapChainImageViews().size());

	// Iterate over all of the views to Get a frame buffer for them.
	for (auto i = 0; i < VulkanManager::GetVulkanManager().GetSwapChainImageViews().size(); ++i)
	{
		std::array<VkImageView, 3> attachments = {
			m_colorImageView,
			m_depthImageView,
			VulkanManager::GetVulkanManager().GetSwapChainImageViews()[i],
		};

		// Frame buffers can only be used with compatible render passes.
		// They roughly have the same attachments.
		
		VkFramebufferCreateInfo frameBufferInfo{};
		{
			frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameBufferInfo.renderPass = m_sampleModel.GetRenderPass();
			frameBufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			frameBufferInfo.pAttachments = attachments.data();
			frameBufferInfo.width = VulkanManager::GetVulkanManager().GetSwapChainExtent().width;
			frameBufferInfo.height = VulkanManager::GetVulkanManager().GetSwapChainExtent().height;
			frameBufferInfo.layers = 1;
		}

		VK_ASSERT(vkCreateFramebuffer(VulkanManager::GetVulkanManager().GetDevice(), &frameBufferInfo, nullptr, &m_swapChainFrameBuffers[i]), "Failed to create frame buffer");
	}

#if IMGUI_ENABLED

	m_imguiManager.CreateFrameBuffers();
#endif
}

void HelloTriangle::CreateCommandPool()
{
	m_sampleModel.CreateCommandPool();
}

void HelloTriangle::CreateVertexBuffer()
{
	VulkanManager::GetVulkanManager().CreateVertexBufer(m_vertices, m_vertexBuffer, m_vertexBufferMemory, m_sampleModel.GetCommandPool());
}

void HelloTriangle::CreateIndexBuffer()
{
	VulkanManager::GetVulkanManager().CreateIndexBuffer(m_indices, m_indexBuffer, m_indexBufferMemory, m_sampleModel.GetCommandPool());
}

void HelloTriangle::CreateUniformBuffers()
{
	VulkanManager::GetVulkanManager().CreateUniformBuffer(m_sampleModel.m_uniformBuffers, m_sampleModel.m_uniformBuffersMemory);
}

void HelloTriangle::CreateDescriptorPools()
{
	// Descriptor sets must be allocated from a command pool.
	m_sampleModel.CreateDescriptorPool();

#if IMGUI_ENABLED
	m_imguiManager.CreateDescriptorPool();
#endif
}

void HelloTriangle::CreateDescriptorSets()
{
	std::vector<VkDescriptorSetLayout> layouts(VulkanManager::GetVulkanManager().GetSwapChainImages().size(), m_sampleModel.GetDescriptorSetLayout());

	VkDescriptorSetAllocateInfo allocInfo{};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_sampleModel.GetDescriptorPool();
		allocInfo.descriptorSetCount = static_cast<uint32_t>(VulkanManager::GetVulkanManager().GetSwapChainImages().size());
		allocInfo.pSetLayouts = layouts.data();
	}

	// Create one descriptor for each image in the swap chain.
	m_descriptorSets.resize(VulkanManager::GetVulkanManager().GetSwapChainImages().size());

	// Each descriptor set that gets allocated will also get a uniform buffer descriptor.
	VK_ASSERT(vkAllocateDescriptorSets(VulkanManager::GetVulkanManager().GetDevice(), &allocInfo, m_descriptorSets.data()), "Failed to allocate descriptor sets");

	// Configure descriptor sets.
	for (size_t i = 0; i < VulkanManager::GetVulkanManager().NumSwapChainImages(); ++i)
	{
		// Specify which buffer we want the descriptor to refer to.
		VkDescriptorBufferInfo bufferInfo{};
		{
			bufferInfo.buffer = m_sampleModel.m_uniformBuffers[i];
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(UniformBufferObject);
		}

		// Bind image and sampler
		VkDescriptorImageInfo imageInfo{};
		{
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = m_sampleModel.m_textureImageView;
			imageInfo.sampler = m_sampleModel.m_textureSampler;
		}

		// Set up for update
		std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
		{
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = m_descriptorSets[i];
			descriptorWrites[0].dstBinding = 0;										// Which slot the buffer is in the shader
			descriptorWrites[0].dstArrayElement = 0;								// First index in array we're updating
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;	// Type of descriptor
			descriptorWrites[0].descriptorCount = 1;								// How many array elements we're updating
			descriptorWrites[0].pBufferInfo = &bufferInfo;							// What data is the descriptor referring to
			descriptorWrites[0].pImageInfo = nullptr;								// What image is the descriptor referring to
			descriptorWrites[0].pTexelBufferView = nullptr;							// What view is the descriptor referring to

			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = m_descriptorSets[i];
			descriptorWrites[1].dstBinding = 1;										// Which slot the buffer is in the shader
			descriptorWrites[1].dstArrayElement = 0;								// First index in array we're updating
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[1].descriptorCount = 1;								// How many array elements we're updating
			descriptorWrites[1].pBufferInfo = nullptr;								// What data is the descriptor referring to
			descriptorWrites[1].pImageInfo = &imageInfo;							// What image is the descriptor referring to
			descriptorWrites[1].pTexelBufferView = nullptr;							// What view is the descriptor referring to
		}
		
		// Update the descriptor set
		vkUpdateDescriptorSets(VulkanManager::GetVulkanManager().GetDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}

void HelloTriangle::CreateCommandBuffers()
{
	// We need to allocate command buffers to be able write commands.
	// Each image in the swap chain needs to have a command buffer written to.
	
	m_commandBuffers.resize(m_swapChainFrameBuffers.size());

	std::array<VkClearValue, 2> clearValues{};
	{
		clearValues[0].color = {0, 0, 0, 1};
		clearValues[1].depthStencil = {1, 0};
	}
	
	VkCommandBufferAllocateInfo allocInfo{};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_sampleModel.GetCommandPool();
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;	// Submit to queue for execution but cannot be called from other command buffers (secondary).
		allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();

		VK_ASSERT(vkAllocateCommandBuffers(VulkanManager::GetVulkanManager().GetDevice(), &allocInfo, m_commandBuffers.data()), "Failed to allocate command buffers");
	}

	// Starting command buffer recording
	for (auto i = 0; i < m_commandBuffers.size(); ++i)
	{
		// Describe how the command buffers are being used.
		VkCommandBufferBeginInfo beginInfo{};
		{
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = 0;	// How to use command buffer.
			beginInfo.pInheritanceInfo = nullptr;	// Only relevant for secondary command buffers - which state to inherit from primary buffer.
		}

		VK_ASSERT(vkBeginCommandBuffer(m_commandBuffers[i], &beginInfo), "Failed to begin recording command buffer");

		// Starting a render pass
		VkRenderPassBeginInfo renderPassInfo{};
		{
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = m_sampleModel.GetRenderPass();
			renderPassInfo.framebuffer = m_swapChainFrameBuffers[i];
			renderPassInfo.renderArea.offset = {0, 0};
			renderPassInfo.renderArea.extent = VulkanManager::GetVulkanManager().GetSwapChainExtent();

			renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
			renderPassInfo.pClearValues = clearValues.data();
		}

		vkCmdBeginRenderPass(	m_commandBuffers[i], 
								&renderPassInfo, 
								VK_SUBPASS_CONTENTS_INLINE	// Details the render pass - It's a primary command buffer, everything will be sent in one go.
		);

		// Basic drawing
		vkCmdBindPipeline(	m_commandBuffers[i], 
							VK_PIPELINE_BIND_POINT_GRAPHICS,	// Graphics or compute pipeline	
							m_sampleModel.m_graphicsPipeline
		);

		// Bind vertex buffer
		VkBuffer vertexBuffers[] = { m_vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(m_commandBuffers[i], 0, 1, vertexBuffers, offsets);

		// Bind index buffer
		vkCmdBindIndexBuffer(m_commandBuffers[i], m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		// Bind descriptor sets
		vkCmdBindDescriptorSets(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_sampleModel.m_pipelineLayout, 0, 1, &m_descriptorSets[i], 0, nullptr);

		// Draw
		vkCmdDrawIndexed(m_commandBuffers[i], static_cast<uint32_t>(m_indices.size()), 1, 0, 0, 0);
		
		// End render pass
		vkCmdEndRenderPass(m_commandBuffers[i]);

		VK_ASSERT(vkEndCommandBuffer(m_commandBuffers[i]), "Failed to record command buffer");
	}

#if IMGUI_ENABLED
	m_imguiManager.CreateCommandPool();
#endif
}

VkCommandBuffer HelloTriangle::BeginSingleTimeCommands()
{
	return VulkanManager::GetVulkanManager().BeginSingleTimeCommands(m_sampleModel.GetCommandPool());
}

void HelloTriangle::EndSingleTimeCommands(VkCommandBuffer commandBuffer)
{
	VulkanManager::GetVulkanManager().EndSingleTimeCommands(commandBuffer, m_sampleModel.GetCommandPool());
}

void HelloTriangle::CreateDepthResources()
{
	VkFormat depthFormat = FindDepthFormat();

	VulkanManager::GetVulkanManager().CreateImage(	VulkanManager::GetVulkanManager().GetSwapChainExtent().width,
	                VulkanManager::GetVulkanManager().GetSwapChainExtent().height,
	                1,
	                VulkanManager::GetVulkanManager().GetMSAASamples(), 
	                depthFormat,
	                VK_IMAGE_TILING_OPTIMAL, 
	                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
	                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depthImage, m_depthImageMemory);

	m_depthImageView = VulkanManager::GetVulkanManager().CreateImageView(m_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

	VulkanManager::GetVulkanManager().TransitionImageLayout(m_sampleModel.GetCommandPool(), m_depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
}

void HelloTriangle::CreateColorResources()
{
	// Create multisampled color buffer

	VkFormat colorFormat = VulkanManager::GetVulkanManager().GetSwapChainImageFormat();

	VulkanManager::GetVulkanManager().CreateImage(
		VulkanManager::GetVulkanManager().GetSwapChainExtent().width, 
		VulkanManager::GetVulkanManager().GetSwapChainExtent().height,
		1, VulkanManager::GetVulkanManager().GetMSAASamples(), 
		colorFormat, 
		VK_IMAGE_TILING_OPTIMAL, 
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
		m_colorImage, 
		m_colorImageMemory);

	m_colorImageView = VulkanManager::GetVulkanManager().CreateImageView(m_colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}

void HelloTriangle::CleanupSwapChain()
{
	m_imguiManager.Cleanup();
	
	vkDestroyImageView(VulkanManager::GetVulkanManager().GetDevice(), m_colorImageView, nullptr);
	vkDestroyImage(VulkanManager::GetVulkanManager().GetDevice(), m_colorImage, nullptr);
	vkFreeMemory(VulkanManager::GetVulkanManager().GetDevice(), m_colorImageMemory, nullptr);
	
	vkDestroyImageView(VulkanManager::GetVulkanManager().GetDevice(), m_depthImageView, nullptr);
	vkDestroyImage(VulkanManager::GetVulkanManager().GetDevice(), m_depthImage, nullptr);
	vkFreeMemory(VulkanManager::GetVulkanManager().GetDevice(), m_depthImageMemory, nullptr);
	
	for (auto& framebuffer : m_swapChainFrameBuffers)
	{
		vkDestroyFramebuffer(VulkanManager::GetVulkanManager().GetDevice(), framebuffer, nullptr);
	}

	vkFreeCommandBuffers(VulkanManager::GetVulkanManager().GetDevice(), m_sampleModel.GetCommandPool(), static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());

	// Moved to Samplemodel
	vkDestroyPipeline(VulkanManager::GetVulkanManager().GetDevice(), m_sampleModel.m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(VulkanManager::GetVulkanManager().GetDevice(), m_sampleModel.m_pipelineLayout, nullptr);
	vkDestroyRenderPass(VulkanManager::GetVulkanManager().GetDevice(), m_sampleModel.GetRenderPass(), nullptr);

	for (auto& imageView : VulkanManager::GetVulkanManager().GetSwapChainImageViews())
	{
		vkDestroyImageView(VulkanManager::GetVulkanManager().GetDevice(), imageView, nullptr);
	}

	vkDestroySwapchainKHR(VulkanManager::GetVulkanManager().GetDevice(), VulkanManager::GetVulkanManager().GetSwapChain(), nullptr);

	for (size_t i = 0; i < VulkanManager::GetVulkanManager().GetSwapChainImages().size(); ++i)
	{
		vkDestroyBuffer(VulkanManager::GetVulkanManager().GetDevice(), m_sampleModel.m_uniformBuffers[i], nullptr);
		vkFreeMemory(VulkanManager::GetVulkanManager().GetDevice(), m_sampleModel.m_uniformBuffersMemory[i], nullptr);
	}

	vkDestroyDescriptorPool(VulkanManager::GetVulkanManager().GetDevice(), m_sampleModel.GetDescriptorPool(), nullptr);
}

void HelloTriangle::Cleanup()
{
	CleanupSwapChain();

	vkDestroySampler(VulkanManager::GetVulkanManager().GetDevice(), m_sampleModel.m_textureSampler, nullptr);
	vkDestroyImageView(VulkanManager::GetVulkanManager().GetDevice(), m_sampleModel.m_textureImageView, nullptr);
	vkDestroyImage(VulkanManager::GetVulkanManager().GetDevice(), m_sampleModel.m_textureImage, nullptr);
	vkFreeMemory(VulkanManager::GetVulkanManager().GetDevice(), m_sampleModel.m_textureImageMemory, nullptr);

	vkDestroyDescriptorSetLayout(VulkanManager::GetVulkanManager().GetDevice(), m_sampleModel.GetDescriptorSetLayout(), nullptr);
	
	vkDestroyBuffer(VulkanManager::GetVulkanManager().GetDevice(), m_vertexBuffer, nullptr);
	vkFreeMemory(VulkanManager::GetVulkanManager().GetDevice(), m_vertexBufferMemory, nullptr);

	vkDestroyBuffer(VulkanManager::GetVulkanManager().GetDevice(), m_indexBuffer, nullptr);
	vkFreeMemory(VulkanManager::GetVulkanManager().GetDevice(), m_indexBufferMemory, nullptr);
	
	for(auto i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		vkDestroySemaphore(VulkanManager::GetVulkanManager().GetDevice(), VulkanManager::GetVulkanManager().GetImageAvailableSemaphores()[i], nullptr);
		vkDestroySemaphore(VulkanManager::GetVulkanManager().GetDevice(), VulkanManager::GetVulkanManager().GetRenderFinishedSemaphores()[i], nullptr);
		vkDestroyFence(VulkanManager::GetVulkanManager().GetDevice(), VulkanManager::GetVulkanManager().GetInFlightFences()[i], nullptr);
	}
	
	vkDestroyCommandPool(VulkanManager::GetVulkanManager().GetDevice(), m_sampleModel.GetCommandPool(), nullptr);
	
	// Device queues (graphics queue) are implicitly destroyed when the device is destroyed.
	vkDestroyDevice(VulkanManager::GetVulkanManager().GetDevice(), nullptr);
	
	if (g_enableValidationLayers)
	{
		DebugLayer::DestroyDebugUtilsMessengerEXT(VulkanManager::GetVulkanManager().GetInstance(), VulkanManager::GetVulkanManager().GetDebugMessenger(), nullptr);
	}

	vkDestroySurfaceKHR(VulkanManager::GetVulkanManager().GetInstance(), VulkanManager::GetVulkanManager().GetSurface(), nullptr);
	
	// Don't destroy physical device since it is destroyed implicitly when the instance is destroyed.
	vkDestroyInstance(VulkanManager::GetVulkanManager().GetInstance(), nullptr);

	glfwDestroyWindow(m_window);
	glfwTerminate();
}

// TODO: this should be in its own class.
void HelloTriangle::LoadModel()
{
	using namespace tinyobj;
	attrib_t attrib;
	std::vector<shape_t> shapes;
	std::vector<material_t> materials;
	std::string warning, error;

	if (!LoadObj(&attrib, &shapes, &materials, &warning, &error, MODEL_PATH.c_str()))
	{
		throw std::runtime_error(warning + error);
	}

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};
	
	for (const auto& shape : shapes)
	{
		for (const auto& index : shape.mesh.indices)
		{
			Vertex vertex{};
			{
				vertex.position = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2],
				};

				vertex.uv = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1 - attrib.texcoords[2 * index.texcoord_index + 1],	// TinyObj does some weird flipping.
				};

				vertex.color = { 1, 1, 1 };
			}

			if (uniqueVertices.count(vertex) == 0) {
				uniqueVertices[vertex] = static_cast<uint32_t>(m_vertices.size());
				m_vertices.push_back(vertex);
			}
			
			m_indices.push_back(uniqueVertices[vertex]);
		}
	}
}
