#include "hello_triangle.h"
#include "vulkan_helper.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <map>
#include <set>
#include <cstdint>
#include <algorithm>
#include <unordered_map>

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
	CreateTextureImage();
	CreateTextureImageView();
	CreateTextureSampler();
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
	// Load shader code
	auto vsCode = ReadFile(SHADER_DIRECTORY + "vert.spv");
	auto fsCode = ReadFile(SHADER_DIRECTORY + "frag.spv");

	// Create modules
	auto vsModule = CreateShaderModule(vsCode);
	auto fsModule = CreateShaderModule(fsCode);

	// Create shader stages
	VkPipelineShaderStageCreateInfo vsStageInfo{};
	{
		vsStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vsStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vsStageInfo.module = vsModule;
		vsStageInfo.pName = "main";	// Specify entry point function
		//vsStageInfo.pSpecializationInfo	// Specify values for shader constants
	}

	VkPipelineShaderStageCreateInfo fsStageInfo{};
	{
		fsStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fsStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fsStageInfo.module = fsModule;
		fsStageInfo.pName = "main";	// Specify entry point function
		//fsStageInfo.pSpecializationInfo	// Specify values for shader constants
	}

	VkPipelineShaderStageCreateInfo shaderStages[] = { vsStageInfo, fsStageInfo };
	
	// Describe the vertex data being passed to the shader
	// Bindings - spacing between data and whether the data is per vertex or per instance
	auto bindingDescription = Vertex::GetBindingDescription();
	// Attribute descriptions - the type of data being passed in and how to load them
	auto attributeDescription = Vertex::GetAttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	{
		// We have no vertex data to pass to the shader so we don't really need to do anything else here.
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data();
	}

	// Input assembly
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	{
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;
	}

	// Viewports
	// Describe the region of the frame buffer to render to
	VkViewport viewport{};
	{
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)VulkanManager::GetVulkanManager().GetSwapChainExtent().width;
		viewport.height = (float)VulkanManager::GetVulkanManager().GetSwapChainExtent().height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
	}

	// Scissors
	// Which regions pixels will be stored.
	// The rasterizer will discard pixels that are outside the scissor.
	VkRect2D scissor{};
	{
		scissor.offset = { 0, 0 };
		scissor.extent = VulkanManager::GetVulkanManager().GetSwapChainExtent();
	}
	
	// Create the viewport
	VkPipelineViewportStateCreateInfo viewportState{};
	{
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;
	}

	// Rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	{
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;	// Setting to true results in geometry never going to the fragment shader
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;	// How fragments are generated for geometry
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_NONE;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;	// Vertex order to be considered front facing
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasSlopeFactor = 0.0f;
	}

	// Multisampling (AA)
	// Combines results of the fragment shader from multiple polygons that write to the same pixel.
	// This only runs if *multiple* polygons are detected.
	VkPipelineMultisampleStateCreateInfo multisampling{};
	{
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VulkanManager::GetVulkanManager().GetMSAASamples();
		multisampling.minSampleShading = 1.0f;
		multisampling.pSampleMask = nullptr;
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;
	}

	// Depth testing
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	{
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0;
		depthStencil.maxDepthBounds = 1;
	}

	// Color blending
	// Describes how to blend results from the fragment shader of a pixel.
	// VkPipelineColorBlendAttachmentState for the attached frame buffer
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	{
		// The idea is:
		// if(blendEnable)
		// {
		//		finalColor = (srcColorBlendFactor * newColor) <blend op> (dstColorBlendFactor * oldColor);
		//		finalAlpha = (srcAlphaBlendFactor * newAlpha) <alpha blend op> (dstAlphaBlendFactor * oldAlpha)
		// }
		// else
		// {
		//		finalColor = newColor;	// just take the new color.
		// }
		// 
		// finalColor = finalColor & colorWriteMask; // determine which channels get written to the buffer
		
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	}
	
	// VkPipelineColorBlendStateCreateInfo for all frame buffers.
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	{
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;
	}

	// Dynamic state
	// Some states can be changed without recreating the pipeline (viewport, blending)
	// We will need to specify these values at each draw call.
	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_LINE_WIDTH,
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	{
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates = dynamicStates;
	}

	// Describe the pipeline
	// Specify which descriptor set the shaders are using.
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	{
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &m_sampleModel.GetDescriptorSetLayout();
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;
	}

	VK_ASSERT(vkCreatePipelineLayout(VulkanManager::GetVulkanManager().GetDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout), "Failed to create pipeline layout");

	// Create graphics pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	{
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;

		// Reference all the structures describing the fixed-function stage.
		{
			pipelineInfo.pVertexInputState = &vertexInputInfo;
			pipelineInfo.pInputAssemblyState = &inputAssembly;
			pipelineInfo.pViewportState = &viewportState;
			pipelineInfo.pRasterizationState = &rasterizer;
			pipelineInfo.pMultisampleState = &multisampling;
			pipelineInfo.pColorBlendState = &colorBlending;
		}

		pipelineInfo.layout = m_pipelineLayout;

		// Reference render pass and the subpass of this graphics pipeline.
		{
			pipelineInfo.renderPass = m_sampleModel.GetRenderPass();
			pipelineInfo.subpass = 0;
		}

		// Any render pass can be used with this pipeline as long as they are compatible with m_sampleModel.GetRenderPass().
		// You can create a new pass by deriving from this pipeline.
		{
			pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
			pipelineInfo.basePipelineIndex = -1;
		}

		pipelineInfo.pDepthStencilState = &depthStencil;
	}

	VK_ASSERT(vkCreateGraphicsPipelines(VulkanManager::GetVulkanManager().GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline), "Failed to create a graphics pipeline");
	
	vkDestroyShaderModule(VulkanManager::GetVulkanManager().GetDevice(), vsModule, nullptr);
	vkDestroyShaderModule(VulkanManager::GetVulkanManager().GetDevice(), fsModule, nullptr);
}

VkShaderModule HelloTriangle::CreateShaderModule(const std::vector<char>& code)
{
	// Wraps the code in a VkShaderModule object.

	VkShaderModuleCreateInfo createInfo{};
	{
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
	}

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(VulkanManager::GetVulkanManager().GetDevice(), &createInfo, nullptr, &shaderModule))
	{
		throw std::runtime_error("Failed to create shader module");
	}

	return shaderModule;
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

void HelloTriangle::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory)
{
	// Create a buffer for CPU to store data in for the GPU to read -----
	VkBufferCreateInfo bufferInfo{};
	{
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		// Buffers can be owned by specific queue families or shared between multiple families.
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;	// We are using this buffer from the graphics queue.

		VK_ASSERT(vkCreateBuffer(VulkanManager::GetVulkanManager().GetDevice(), &bufferInfo, nullptr, &buffer), "Failed to create vertex buffer");
	}

	// Figure out how much memory we need to load -----
	VkMemoryRequirements memoryRequirements;
	{
		vkGetBufferMemoryRequirements(VulkanManager::GetVulkanManager().GetDevice(), buffer, &memoryRequirements);
	}
	
	// Memory allocation -----
	VkMemoryAllocateInfo allocInfo{};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memoryRequirements.size;
		// FINDING THE RIGHT MEMORY TYPE IS VERY VERY VERY IMPORTANT TO MAP BUFFERS.
		allocInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, properties);

		// Store handle to memory
		VK_ASSERT(vkAllocateMemory(VulkanManager::GetVulkanManager().GetDevice(), &allocInfo, nullptr, &memory), "Failed to allocate vertex buffer memory");
	}

	// Bind the buffer memory -----
	// We're allocating specifically for this vertex buffer, no offset.
	// Offset has to be divisible by memoryRequirements.alignment otherwise.
	VK_ASSERT(vkBindBufferMemory(VulkanManager::GetVulkanManager().GetDevice(), buffer, memory, 0), "Failed to bind vertex buffer");
}

// Copy from srcBuffer to dstBuffer.
void HelloTriangle::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	// Transfer operations are done through command buffers.
	// This can be a temporary command buffer or through the command pool.

	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	VkBufferCopy copyRegion{};
	{
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = size;
	}
	
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	EndSingleTimeCommands(commandBuffer);
}

void HelloTriangle::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	VkBufferImageCopy region{};
	{
		region.bufferOffset = 0;	// Offset where pixels start

		// How pixels are laid out in memory
		// Zero means pixels are tightly packed.
		// Nonzero means the pixels are padded.
		{
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
		}
		
		// Which region of the image should be copied
		{
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageOffset = { 0, 0, 0 };
			region.imageExtent = { width, height, 1 };
		}
	}

	vkCmdCopyBufferToImage(
		commandBuffer, 
		buffer, 
		image, 
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// Which layout the image is currently using
		1,
		&region);
	
	EndSingleTimeCommands(commandBuffer);
}

void HelloTriangle::CreateVertexBuffer()
{
	auto bufferSize = sizeof(Vertex) * m_vertices.size();

	// Create a temporary buffer -----
	// Use the staging buffer to map and copy vertex data.
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	
	// VK_BUFFER_USAGE_TRANSFER_SRC_BIT - Use this buffer as the source in transferring memory.
	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
	
	// Fill the vertex buffer -----
	void* data;
	{
		// Map buffer memory into CPU accessible memory.
		VK_ASSERT(vkMapMemory(VulkanManager::GetVulkanManager().GetDevice(), stagingBufferMemory, 0, bufferSize, 0, &data), "Failed to map data to vertex buffer");
		
		// Copy data over
		memcpy(data, m_vertices.data(), (size_t)bufferSize);
		
		// Unmap
		vkUnmapMemory(VulkanManager::GetVulkanManager().GetDevice(), stagingBufferMemory);
	}

	// VK_BUFFER_USAGE_TRANSFER_DST_BIT - Use this buffer as the destination in transferring memory.
	// VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT - The most optimal use of memory. We need to use a staging buffer for this since it's not directly accessible with CPU.
	// The vertex buffer is device local. This means that we can't map memory directly to it but we can copy data from another buffer over.
	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertexBuffer, m_vertexBufferMemory);

	// Copy from stage to vertex buffer
	CopyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);

	// Destroy staging buffer
	{
		vkDestroyBuffer(VulkanManager::GetVulkanManager().GetDevice(), stagingBuffer, nullptr);
		vkFreeMemory(VulkanManager::GetVulkanManager().GetDevice(), stagingBufferMemory, nullptr);
	}
}

void HelloTriangle::CreateIndexBuffer()
{
	VkDeviceSize bufferSize = sizeof(m_indices[0]) * m_indices.size();

	// Create a temporary buffer -----
	// Use the staging buffer to map and copy index data.
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	// VK_BUFFER_USAGE_TRANSFER_SRC_BIT - Use this buffer as the source in transferring memory.
	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	// Fill the index buffer -----
	void* data;
	{
		// Map buffer memory into CPU accessible memory.
		VK_ASSERT(vkMapMemory(VulkanManager::GetVulkanManager().GetDevice(), stagingBufferMemory, 0, bufferSize, 0, &data), "Failed to map data to index buffer");

		// Copy data over
		memcpy(data, m_indices.data(), (size_t)bufferSize);

		// Unmap
		vkUnmapMemory(VulkanManager::GetVulkanManager().GetDevice(), stagingBufferMemory);
	}

	// VK_BUFFER_USAGE_TRANSFER_DST_BIT - Use this buffer as the destination in transferring memory.
	// VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT - The most optimal use of memory. We need to use a staging buffer for this since it's not directly accessible with CPU.
	// The vertex buffer is device local. This means that we can't map memory directly to it but we can copy data from another buffer over.
	CreateBuffer(bufferSize, 
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
		m_indexBuffer, 
		m_indexBufferMemory);

	// Copy from stage to vertex buffer
	CopyBuffer(stagingBuffer, m_indexBuffer, bufferSize);

	// Destroy staging buffer
	{
		vkDestroyBuffer(VulkanManager::GetVulkanManager().GetDevice(), stagingBuffer, nullptr);
		vkFreeMemory(VulkanManager::GetVulkanManager().GetDevice(), stagingBufferMemory, nullptr);
	}
}

void HelloTriangle::UpdateUniformBuffers(uint32_t currentImage)
{
	m_sampleModel.UpdateUniformBuffers(currentImage);
}

void HelloTriangle::CreateUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);
	{
		m_sampleModel.m_uniformBuffers.resize(VulkanManager::GetVulkanManager().GetSwapChainImages().size());
		m_sampleModel.m_uniformBuffersMemory.resize(VulkanManager::GetVulkanManager().GetSwapChainImages().size());
	}

	for (size_t i = 0; i < VulkanManager::GetVulkanManager().GetSwapChainImages().size(); ++i)
	{
		CreateBuffer(bufferSize, 
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			m_sampleModel.m_uniformBuffers[i],
			m_sampleModel.m_uniformBuffersMemory[i]);
	}
}

void HelloTriangle::CreateDescriptorPools()
{
	CreateMainDescriptorPool();

#if IMGUI_ENABLED
	m_imguiManager.CreateDescriptorPool();
#endif
}

void HelloTriangle::CreateMainDescriptorPool()
{
	// Descriptor sets must be allocated from a command pool.
	
	m_sampleModel.CreateDescriptorPool();
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
			imageInfo.imageView = m_textureImageView;
			imageInfo.sampler = m_textureSampler;
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

uint32_t HelloTriangle::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	// Query the types of memory

	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(VulkanManager::GetVulkanManager().GetPhysicalDevice(), &memoryProperties);

	// Find the memory type that is suitable with the buffer and make sure it has the properties to do so.
	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
	{
		if (typeFilter & (1 << i) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	ASSERT(false, "Failed to find suitable memory type");

	return -1;
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
							m_graphicsPipeline
		);

		// Bind vertex buffer
		VkBuffer vertexBuffers[] = { m_vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(m_commandBuffers[i], 0, 1, vertexBuffers, offsets);

		// Bind index buffer
		vkCmdBindIndexBuffer(m_commandBuffers[i], m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		// Bind descriptor sets
		vkCmdBindDescriptorSets(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[i], 0, nullptr);

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
	VkCommandBufferAllocateInfo allocInfo{};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = m_sampleModel.GetCommandPool();
		allocInfo.commandBufferCount = 1;
	}

	VkCommandBuffer commandBuffer;
	VK_ASSERT(vkAllocateCommandBuffers(VulkanManager::GetVulkanManager().GetDevice(), &allocInfo, &commandBuffer), "Failed to allocate command buffers");

	VkCommandBufferBeginInfo beginInfo{};
	{
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	}

	VK_ASSERT(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin command buffer");

	return commandBuffer;
}

void HelloTriangle::EndSingleTimeCommands(VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	// Stop recording since we already recorded the copy command.
	VkSubmitInfo submitInfo{};
	{
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
	}

	VK_ASSERT(vkQueueSubmit(VulkanManager::GetVulkanManager().GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit queue");

	// Wait for the transfer to finish.
	VK_ASSERT(vkQueueWaitIdle(VulkanManager::GetVulkanManager().GetGraphicsQueue()), "Failed to wait");

	vkFreeCommandBuffers(VulkanManager::GetVulkanManager().GetDevice(), m_sampleModel.GetCommandPool(), 1, &commandBuffer);
}

void HelloTriangle::CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples,
                                VkFormat format, VkImageTiling tiliing, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
	VkImageCreateInfo imageInfo{};
	{
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = static_cast<uint32_t>(width);
		imageInfo.extent.height = static_cast<uint32_t>(height);
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = mipLevels;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = tiliing;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = numSamples;
		imageInfo.flags = 0;
	}

	// Create the image
	VK_ASSERT(vkCreateImage(VulkanManager::GetVulkanManager().GetDevice(), &imageInfo, nullptr, &image), "Failed to create image");

	VkMemoryRequirements memoryRequirements{};
	vkGetImageMemoryRequirements(VulkanManager::GetVulkanManager().GetDevice(), image, &memoryRequirements);

	VkMemoryAllocateInfo allocInfo{};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memoryRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, properties);
	}

	VK_ASSERT(vkAllocateMemory(VulkanManager::GetVulkanManager().GetDevice(), &allocInfo, nullptr, &imageMemory), "Failed to allocate image memory");

	vkBindImageMemory(VulkanManager::GetVulkanManager().GetDevice(), image, imageMemory, 0);
}

void HelloTriangle::CreateTextureImage()
{
	int texWidth, texHeight, texChannels;
	// Get image
	stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	VkDeviceSize imageSize = texWidth * texHeight * 4;	// Multiply by 4 for each channel RGBA
	
	// Calculate mip levels
	{
		// Find largest dimension
		auto maxDimension = std::max(texWidth, texHeight);
		// Determine how many times the dimension can be divided by 2
		auto divisibleBy2 = std::log2(maxDimension);
		// Make sure result is a power of 2
		auto mipLevels= std::floor(divisibleBy2);
		// Add 1 so the original image has a mip level
		m_mipLevels = static_cast<uint32_t>(mipLevels + 1);
	}
	
	if (!pixels)
	{
		ASSERT(false, "Failed to load texture image");
	}
	
	// Create buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	CreateBuffer(	imageSize, 
					VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
					stagingBuffer, 
					stagingBufferMemory);

	// TODO: this should be its own function
	// Map to temp buffer
	{
		void* data;
		VK_ASSERT(vkMapMemory(VulkanManager::GetVulkanManager().GetDevice(), stagingBufferMemory, 0, imageSize, 0, &data), "Failed to map buffer");
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(VulkanManager::GetVulkanManager().GetDevice(), stagingBufferMemory);
	}

	// We loaded everything into data so we can now clean up pixels.
	stbi_image_free(pixels);

	// With mipmapping enabled, source has to also be VK_IMAGE_USAGE_TRANSFER_SRC_BIT.
	auto usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	
	// Create the image
	CreateImage(texWidth, 
	            texHeight,
	            m_mipLevels,
	            VK_SAMPLE_COUNT_1_BIT, 
	            VK_FORMAT_R8G8B8A8_SRGB, 
	            VK_IMAGE_TILING_OPTIMAL,
	            usage, 
	            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
	            m_textureImage, m_textureImageMemory);

	// Copy the staging buffer to the image
	TransitionImageLayout(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_mipLevels);
	CopyBufferToImage(stagingBuffer, m_textureImage, texWidth, texHeight);

	vkDestroyBuffer(VulkanManager::GetVulkanManager().GetDevice(), stagingBuffer, nullptr);
	vkFreeMemory(VulkanManager::GetVulkanManager().GetDevice()	, stagingBufferMemory, nullptr);
	
	// Generating mipmaps transitions the layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
	GenerateMipmaps(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, m_mipLevels);
}

void HelloTriangle::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
                                          VkImageLayout newLayout, uint32_t mipLevels)
{
	// Transition image from old layout to new layout

	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	// Use a barrier to sync access to resources, transition image layouts, transfer queue family ownership.
	// It's important to be explicit in what you're doing with the resource.

	VkImageMemoryBarrier barrier{};
	{
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;

		// Which part of the image are we syncing
		{
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = mipLevels;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;

			if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
			{
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

				if (HasStencilComponent(format))
				{
					barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
				}
			}
			else
			{
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			}
		}
	}

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;
	
	// Transition rules:
	// Undefined --> transfer destination
	// Transfer destination --> shader read
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		// TODO: read up more on this
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;	// Earliest pipeline stage
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else
	{
		ASSERT(false, "Unsupported layout transition");
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage,		// Which stage the operations should occur before the barrier
		destinationStage,	// Which stage should wait on the barrier
		0,	
		0,
		nullptr,
		0,
		nullptr,
		1,
		&barrier);
	
	EndSingleTimeCommands(commandBuffer);
}

void HelloTriangle::CreateTextureImageView()
{
	// Create a view in order to access images just like with the swap chain.
	m_textureImageView = VulkanManager::GetVulkanManager().CreateImageView(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, m_mipLevels);
}

void HelloTriangle::CreateTextureSampler()
{
	// Textures should be accessed through a sampler.
	// Samplers will apply filtering and transformations.

	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(VulkanManager::GetVulkanManager().GetPhysicalDevice(), &properties);
	
	VkSamplerCreateInfo samplerInfo{};
	{
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

		// How to interpolate texels
		{
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
		}

		// Clamp, Wrap
		{
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		}

		// Limit the amount of texel samples used to calculate the final color.
		{
			samplerInfo.anisotropyEnable = VK_TRUE;
			samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		}

		// Which color is returned when sampling beyond the image.
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

		// Which coordinate system to use to address texels the image.
		// TRUE -> [0, texWidth) and [0, texHeight)
		// FALSE -> [0, 1)
		samplerInfo.unnormalizedCoordinates = VK_FALSE;

		// Texels will be compared to a value and the result is used for filtering.
		// TODO: future chapter.
		{
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		}

		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = static_cast<float>(m_mipLevels);
	}

	// The sampler is distinct from the image. The sampler is a way to get data from a texture so we don't need to ref the image here.
	// This is different than other APIs which requires referring to the actual image.
	VK_ASSERT(vkCreateSampler(VulkanManager::GetVulkanManager().GetDevice(), &samplerInfo, nullptr, &m_textureSampler), "Couldn't create texture sampler");
}

void HelloTriangle::GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
	// Check if format supports linear blitting. Not all platforms support this.
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(VulkanManager::GetVulkanManager().GetPhysicalDevice(), imageFormat, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
	{
		throw std::runtime_error("No support for linear blitting");
	}
	
	VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

	VkImageMemoryBarrier barrier{};
	{
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;
	}

	auto mipWidth = texWidth;
	auto mipHeight = texHeight;

	for (uint32_t i = 1; i < mipLevels; ++i)
	{
		auto currentMipLevel = i - 1;
		auto nextMipLevel = i;
		
		barrier.subresourceRange.baseMipLevel = currentMipLevel;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(
			commandBuffer, 
			VK_PIPELINE_STAGE_TRANSFER_BIT, 
			VK_PIPELINE_STAGE_TRANSFER_BIT, 
			0, 
			0, 
			nullptr, 
			0, 
			nullptr, 
			1, 
			&barrier);

		// Transition current mip level to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
		VkImageBlit blit{};
		{
			// Specify regions of the blit.

			{
				blit.srcOffsets[0] = {0,0,0};
				blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
				blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.srcSubresource.mipLevel = currentMipLevel;
				blit.srcSubresource.baseArrayLayer = 0;
				blit.srcSubresource.layerCount = 1;
			}

			{
				blit.dstOffsets[0] = {0,0,0};
				blit.dstOffsets[1] = {
					mipWidth > 1 ? mipWidth / 2 : 1,		// Divide by two since each mip level is half the previous.
					mipHeight > 1 ? mipHeight / 2 : 1,
					1
				};
				blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blit.dstSubresource.mipLevel = nextMipLevel;
				blit.dstSubresource.baseArrayLayer = 0;
				blit.dstSubresource.layerCount = 1;
			}
		}

		// Blitting between different levels of the same image, source and destination are the same.
		vkCmdBlitImage(
			commandBuffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&blit,
			VK_FILTER_LINEAR);

		// Transition from current mip level to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		{
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(
				commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);
		}

		if (mipWidth > 1)
			mipWidth /= 2;
		if (mipHeight > 1)
			mipHeight /= 2;
	}

	// Transition the last mip level.
	{
		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier);
	}

	EndSingleTimeCommands(commandBuffer);
}

void HelloTriangle::CreateDepthResources()
{
	VkFormat depthFormat = FindDepthFormat();

	CreateImage(	VulkanManager::GetVulkanManager().GetSwapChainExtent().width, 
	                VulkanManager::GetVulkanManager().GetSwapChainExtent().height,
	                1,
	                VulkanManager::GetVulkanManager().GetMSAASamples(), 
	                depthFormat,
	                VK_IMAGE_TILING_OPTIMAL, 
	                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
	                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depthImage, m_depthImageMemory);

	m_depthImageView = VulkanManager::GetVulkanManager().CreateImageView(m_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

	TransitionImageLayout(m_depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
}

//VkFormat HelloTriangle::FindDepthFormat()
//{
//	return FindSupportedFormat(
//		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
//		VK_IMAGE_TILING_OPTIMAL,
//		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
//}

bool HelloTriangle::HasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void HelloTriangle::CreateColorResources()
{
	// Create multisampled color buffer

	VkFormat colorFormat = VulkanManager::GetVulkanManager().GetSwapChainImageFormat();

	CreateImage(
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

	vkDestroyPipeline(VulkanManager::GetVulkanManager().GetDevice(), m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(VulkanManager::GetVulkanManager().GetDevice(), m_pipelineLayout, nullptr);
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

	vkDestroySampler(VulkanManager::GetVulkanManager().GetDevice(), m_textureSampler, nullptr);
	vkDestroyImageView(VulkanManager::GetVulkanManager().GetDevice(), m_textureImageView, nullptr);
	vkDestroyImage(VulkanManager::GetVulkanManager().GetDevice(), m_textureImage, nullptr);
	vkFreeMemory(VulkanManager::GetVulkanManager().GetDevice(), m_textureImageMemory, nullptr);

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
