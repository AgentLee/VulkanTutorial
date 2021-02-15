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

	m_vkManager = VulkanManager(m_window);
	m_vkManager.Initialize();
	
	m_imguiManager.m_vkManager = m_vkManager;
	
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

	vkDeviceWaitIdle(m_vkManager.GetDevice());
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

	vkWaitForFences(m_vkManager.GetDevice(), 1, &m_vkManager.GetInFlightFences()[m_currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;	// Store index of the image from the swap chain.
	auto acquireResult = vkAcquireNextImageKHR(m_vkManager.GetDevice(), m_vkManager.GetSwapChain(), UINT64_MAX, m_vkManager.GetImageAvailableSemaphores()[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

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
	if (m_vkManager.GetImagesInFlight()[imageIndex] != VK_NULL_HANDLE)
	{
		vkWaitForFences(m_vkManager.GetDevice(), 1, &m_vkManager.GetImagesInFlight()[imageIndex], VK_TRUE, UINT64_MAX);
	}

	// Mark image as being used
	m_vkManager.GetImagesInFlight()[imageIndex] = m_vkManager.GetInFlightFences()[m_currentFrame];

	VkSemaphore waitSemaphores[] = { m_vkManager.GetImageAvailableSemaphores()[m_currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSemaphore signalSemaphores[] = { m_vkManager.GetRenderFinishedSemaphores()[m_currentFrame] };

	// Update uniforms
	UpdateUniformBuffers(imageIndex);

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

		vkResetFences(m_vkManager.GetDevice(), 1, &m_vkManager.GetImagesInFlight()[m_currentFrame]);

		VK_ASSERT(vkQueueSubmit(m_vkManager.GetGraphicsQueue(), 1, &submitInfo, m_vkManager.GetImagesInFlight()[m_currentFrame]), "Failed to submit draw command buffer");
	}
	
	// Present
	// Submit result back to swap chain to show on screen.

	VkSwapchainKHR swapChains[] = { m_vkManager.GetSwapChain() };
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

	vkQueuePresentKHR(m_vkManager.GetPresentQueue(), &presentInfo);

	// Wait for work to finish after submission.
	vkQueueWaitIdle(m_vkManager.GetPresentQueue());

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

	auto queueFamily = m_vkManager.FindQueueFamilies(m_vkManager.GetPhysicalDevice());
	auto swapChainSupport = m_vkManager.QuerySwapChainSupport(m_vkManager.GetPhysicalDevice());
	
	ImGui_ImplVulkan_InitInfo init_info = {};
	{
		init_info.Instance = m_vkManager.GetInstance();
		init_info.PhysicalDevice = m_vkManager.GetPhysicalDevice();
		init_info.Device = m_vkManager.GetDevice();
		init_info.QueueFamily = queueFamily.graphicsFamily.value();
		init_info.Queue = m_vkManager.GetGraphicsQueue();
		init_info.PipelineCache = nullptr;
		init_info.DescriptorPool = m_imguiManager.GetDescriptorPool();
		init_info.Allocator = nullptr;
		init_info.MinImageCount = swapChainSupport.capabilities.minImageCount + 1;
		init_info.ImageCount = m_vkManager.NumSwapChainImages();
		init_info.CheckVkResultFn = nullptr;
	}
	ImGui_ImplVulkan_Init(&init_info, m_imguiManager.GetRenderPass());

	// Upload Fonts
	{
		// Use any command queue
		VkCommandPool command_pool = m_imguiManager.GetCommandPool();
		VK_ASSERT(vkResetCommandPool(m_vkManager.GetDevice(), command_pool, 0), "");
		
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
		vkQueueSubmit(m_vkManager.GetGraphicsQueue(), 1, &end_info, VK_NULL_HANDLE);

		vkDeviceWaitIdle(m_vkManager.GetDevice());
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

VkFormat HelloTriangle::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
	VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(m_vkManager.GetPhysicalDevice(), format, &properties);

		if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features)
		{
			return format;
		}
	}

	throw std::runtime_error("Failed to find supported format!");
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
	
	vkDeviceWaitIdle(m_vkManager.GetDevice());

	CleanupSwapChain();
	
	m_vkManager.CreateSwapChain();
	m_vkManager.CreateImageViews();
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

//void HelloTriangle::CreateImageViews()
//{
//	m_vkManager.CreateImageViews();
//}

void HelloTriangle::CreateDescriptorSetLayout()
{
	// Descriptors allow shaders to access buffers and images.
	// Descriptor Layouts specify the type of resources being used by the shader.
	// Descriptor Sets specify the buffer or image that get bound to the descriptor (frame buffers specify image views to render pass attachments)

	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	{
		uboLayoutBinding.binding = 0;											// Binding located in the shader
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;	// Type of descriptor
		uboLayoutBinding.descriptorCount = 1;									// How many descriptors
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;				// Which stage to use the descriptor
		uboLayoutBinding.pImmutableSamplers = nullptr;							// Image sampling descriptor
	}

	// We need to create a combined image sampler. This way teh shader can access the image through the sampler.
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	{
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;	// When do you want to use the image			
		samplerLayoutBinding.pImmutableSamplers = nullptr;
	}

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	{
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());;
		layoutInfo.pBindings = bindings.data();
	}

	VK_ASSERT(vkCreateDescriptorSetLayout(m_vkManager.GetDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout), "Failed to create descriptor set layout");
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
		viewport.width = (float)m_vkManager.GetSwapChainExtent().width;
		viewport.height = (float)m_vkManager.GetSwapChainExtent().height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
	}

	// Scissors
	// Which regions pixels will be stored.
	// The rasterizer will discard pixels that are outside the scissor.
	VkRect2D scissor{};
	{
		scissor.offset = { 0, 0 };
		scissor.extent = m_vkManager.GetSwapChainExtent();
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
		multisampling.rasterizationSamples = m_vkManager.GetMSAASamples();
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
		pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;
	}

	VK_ASSERT(vkCreatePipelineLayout(m_vkManager.GetDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout), "Failed to create pipeline layout");

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
			pipelineInfo.renderPass = m_renderPass;
			pipelineInfo.subpass = 0;
		}

		// Any render pass can be used with this pipeline as long as they are compatible with m_renderPass.
		// You can create a new pass by deriving from this pipeline.
		{
			pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
			pipelineInfo.basePipelineIndex = -1;
		}

		pipelineInfo.pDepthStencilState = &depthStencil;
	}

	VK_ASSERT(vkCreateGraphicsPipelines(m_vkManager.GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline), "Failed to create a graphics pipeline");
	
	vkDestroyShaderModule(m_vkManager.GetDevice(), vsModule, nullptr);
	vkDestroyShaderModule(m_vkManager.GetDevice(), fsModule, nullptr);
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
	if (vkCreateShaderModule(m_vkManager.GetDevice(), &createInfo, nullptr, &shaderModule))
	{
		throw std::runtime_error("Failed to create shader module");
	}

	return shaderModule;
}

void HelloTriangle::CreateRenderPasses()
{
	CreateRenderPass();
	
#if IMGUI_ENABLED
	m_imguiManager.CreateRenderPass();
#endif
}

void HelloTriangle::CreateRenderPass()
{
	// Tell Vulkan about the frame buffer attachments used for rendering.
	// This includes how many color/depth buffers, how many samples for each, and blending.
	// We encapsulate that in a render pass object.

	VkAttachmentDescription colorAttachment{};
	CreateAttachmentDescription(
		colorAttachment,
		m_vkManager.GetSwapChainImageFormat(),
		0,
		m_vkManager.GetMSAASamples(), 
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// Clear buffer for next frame.
		VK_ATTACHMENT_STORE_OP_STORE,				// Save for later since we want to see the triangle.
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// Clear buffer for next frame.
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// Save for later since we want to see the triangle.
		VK_IMAGE_LAYOUT_UNDEFINED,					// We don't care about the previous image since we clear it anyway.
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// The image should be ready to present in the swap chain.
	);

	VkAttachmentDescription depthAttachment{};
	CreateAttachmentDescription(
		depthAttachment,
		FindDepthFormat(),
		0,
		m_vkManager.GetMSAASamples(),
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	);

	// Multisampled images cannot be presented directly, needs to be resolved to a regular image.
	// In order to present it we need to add a new attachment.
	VkAttachmentDescription colorAttachmentResolve{};
	CreateAttachmentDescription(
		colorAttachmentResolve,
		m_vkManager.GetSwapChainImageFormat(),
		0,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	);

	// Subpasses - passes that rely on the previous pass.
	VkAttachmentReference colorAttachmentRef{};
	CreateAttachmentReference(colorAttachmentRef, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	VkAttachmentReference depthAttachmentRef{};
	CreateAttachmentReference(depthAttachmentRef, 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	VkAttachmentReference colorAttachmentResolveRef{};
	CreateAttachmentReference(colorAttachmentResolveRef, 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	std::array<VkAttachmentReference, 1> colorAttachments = {
		colorAttachmentRef,
	};

	VkSubpassDescription subpass{};
	{
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
		subpass.pColorAttachments = colorAttachments.data();
		subpass.pDepthStencilAttachment = &depthAttachmentRef;
		subpass.pResolveAttachments = &colorAttachmentResolveRef;

		// Not too sure about this...
		// The index of the attachment in this array is directly referenced from the fragment shader with
		// the layout(location = 0) out vec4 outColor directive!
	}

	VkSubpassDependency dependency{};
	CreateSubpassDependency(
		dependency, 
		0, 
		VK_SUBPASS_EXTERNAL, 
		0, 
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 
		0, 
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
	);

	std::array<VkAttachmentDescription, 3> attachments = {
		colorAttachment,
		depthAttachment,
		colorAttachmentResolve,
	};

	// Create the render pass
	VkRenderPassCreateInfo renderPassInfo{};
	{
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;
	}

	VK_ASSERT(vkCreateRenderPass(m_vkManager.GetDevice(), &renderPassInfo, nullptr, &m_renderPass), "Failed to create render pass");
}

void HelloTriangle::CreateFrameBuffers()
{
	// Each FBO will need to reference each of the image view objects.

	m_swapChainFrameBuffers.resize(m_vkManager.GetSwapChainImageViews().size());
	m_imguiManager.GetFrameBuffers().resize(m_vkManager.GetSwapChainImageViews().size());

	// Iterate over all of the views to Get a frame buffer for them.
	for (auto i = 0; i < m_vkManager.GetSwapChainImageViews().size(); ++i)
	{
		std::array<VkImageView, 3> attachments = {
			m_colorImageView,
			m_depthImageView,
			m_vkManager.GetSwapChainImageViews()[i],
		};

		// Frame buffers can only be used with compatible render passes.
		// They roughly have the same attachments.
		
		VkFramebufferCreateInfo frameBufferInfo{};
		{
			frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameBufferInfo.renderPass = m_renderPass;
			frameBufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			frameBufferInfo.pAttachments = attachments.data();
			frameBufferInfo.width = m_vkManager.GetSwapChainExtent().width;
			frameBufferInfo.height = m_vkManager.GetSwapChainExtent().height;
			frameBufferInfo.layers = 1;
		}

		VK_ASSERT(vkCreateFramebuffer(m_vkManager.GetDevice(), &frameBufferInfo, nullptr, &m_swapChainFrameBuffers[i]), "Failed to create frame buffer");
	}

#if IMGUI_ENABLED
	{
		VkImageView attachment[1];
		VkFramebufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		info.renderPass = m_imguiManager.GetRenderPass();
		info.attachmentCount = 1;
		info.pAttachments = attachment;
		info.width = m_vkManager.GetSwapChainExtent().width;
		info.height = m_vkManager.GetSwapChainExtent().height;
		info.layers = 1;
		for (uint32_t i = 0; i < m_imguiManager.GetFrameBuffers().size(); ++i)
		{
			attachment[0] = m_vkManager.GetSwapChainImageViews()[i];
			vkCreateFramebuffer(m_vkManager.GetDevice(), &info, nullptr, &m_imguiManager.GetFrameBuffers()[i]);
		}
	}
#endif
}

void HelloTriangle::CreateCommandPool()
{
	auto queueFamilyIndices = m_vkManager.FindQueueFamilies(m_vkManager.GetPhysicalDevice());

	VkCommandPoolCreateInfo poolInfo{};
	{
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();	// We want to draw, use the graphics family
		poolInfo.flags = 0;
	}

	VK_ASSERT(vkCreateCommandPool(m_vkManager.GetDevice(), &poolInfo, nullptr, &m_commandPool), "Failed to create command pool");
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

		VK_ASSERT(vkCreateBuffer(m_vkManager.GetDevice(), &bufferInfo, nullptr, &buffer), "Failed to create vertex buffer");
	}

	// Figure out how much memory we need to load -----
	VkMemoryRequirements memoryRequirements;
	{
		vkGetBufferMemoryRequirements(m_vkManager.GetDevice(), buffer, &memoryRequirements);
	}
	
	// Memory allocation -----
	VkMemoryAllocateInfo allocInfo{};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memoryRequirements.size;
		// FINDING THE RIGHT MEMORY TYPE IS VERY VERY VERY IMPORTANT TO MAP BUFFERS.
		allocInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, properties);

		// Store handle to memory
		VK_ASSERT(vkAllocateMemory(m_vkManager.GetDevice(), &allocInfo, nullptr, &memory), "Failed to allocate vertex buffer memory");
	}

	// Bind the buffer memory -----
	// We're allocating specifically for this vertex buffer, no offset.
	// Offset has to be divisible by memoryRequirements.alignment otherwise.
	VK_ASSERT(vkBindBufferMemory(m_vkManager.GetDevice(), buffer, memory, 0), "Failed to bind vertex buffer");
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
		VK_ASSERT(vkMapMemory(m_vkManager.GetDevice(), stagingBufferMemory, 0, bufferSize, 0, &data), "Failed to map data to vertex buffer");
		
		// Copy data over
		memcpy(data, m_vertices.data(), (size_t)bufferSize);
		
		// Unmap
		vkUnmapMemory(m_vkManager.GetDevice(), stagingBufferMemory);
	}

	// VK_BUFFER_USAGE_TRANSFER_DST_BIT - Use this buffer as the destination in transferring memory.
	// VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT - The most optimal use of memory. We need to use a staging buffer for this since it's not directly accessible with CPU.
	// The vertex buffer is device local. This means that we can't map memory directly to it but we can copy data from another buffer over.
	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertexBuffer, m_vertexBufferMemory);

	// Copy from stage to vertex buffer
	CopyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);

	// Destroy staging buffer
	{
		vkDestroyBuffer(m_vkManager.GetDevice(), stagingBuffer, nullptr);
		vkFreeMemory(m_vkManager.GetDevice(), stagingBufferMemory, nullptr);
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
		VK_ASSERT(vkMapMemory(m_vkManager.GetDevice(), stagingBufferMemory, 0, bufferSize, 0, &data), "Failed to map data to index buffer");

		// Copy data over
		memcpy(data, m_indices.data(), (size_t)bufferSize);

		// Unmap
		vkUnmapMemory(m_vkManager.GetDevice(), stagingBufferMemory);
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
		vkDestroyBuffer(m_vkManager.GetDevice(), stagingBuffer, nullptr);
		vkFreeMemory(m_vkManager.GetDevice(), stagingBufferMemory, nullptr);
	}
}

void HelloTriangle::UpdateUniformBuffers(uint32_t currentImage)
{
	float time = GetCurrentTime();
	time = 1.0f;

	UniformBufferObject ubo{};
	{
		ubo.model = glm::rotate(glm::mat4(1), time * glm::radians(0.0f), glm::vec3(0, 0, 1));
		ubo.view = glm::lookAt(glm::vec3(2, 2, 2), glm::vec3(0), glm::vec3(0, 0, 1));
		ubo.proj = glm::perspective(glm::radians(45.0f), (float)m_vkManager.GetSwapChainExtent().width / (float)m_vkManager.GetSwapChainExtent().height, 0.1f, 10.0f);
		ubo.proj[1][1] *= -1.0f;
	}

	void* data;
	VK_ASSERT(vkMapMemory(m_vkManager.GetDevice(), m_uniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data), "Couldn't map uniform buffer");
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(m_vkManager.GetDevice(), m_uniformBuffersMemory[currentImage]);
}

void HelloTriangle::CreateUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);
	{
		m_uniformBuffers.resize(m_vkManager.GetSwapChainImages().size());
		m_uniformBuffersMemory.resize(m_vkManager.GetSwapChainImages().size());
	}

	for (size_t i = 0; i < m_vkManager.GetSwapChainImages().size(); ++i)
	{
		CreateBuffer(bufferSize, 
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			m_uniformBuffers[i], 
			m_uniformBuffersMemory[i]);
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
	
	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	{
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = static_cast<uint32_t>(m_vkManager.GetSwapChainImages().size());
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = static_cast<uint32_t>(m_vkManager.GetSwapChainImages().size());
	}

	// Allocate one descriptor every frame.
	VKCreateDescriptorPool(m_vkManager.GetDevice(), &m_descriptorPool, poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), static_cast<uint32_t>(m_vkManager.NumSwapChainImages()));
}

void HelloTriangle::CreateDescriptorSets()
{
	std::vector<VkDescriptorSetLayout> layouts(m_vkManager.GetSwapChainImages().size(), m_descriptorSetLayout);

	VkDescriptorSetAllocateInfo allocInfo{};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_descriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(m_vkManager.GetSwapChainImages().size());
		allocInfo.pSetLayouts = layouts.data();
	}

	// Create one descriptor for each image in the swap chain.
	m_descriptorSets.resize(m_vkManager.GetSwapChainImages().size());

	// Each descriptor set that gets allocated will also get a uniform buffer descriptor.
	VK_ASSERT(vkAllocateDescriptorSets(m_vkManager.GetDevice(), &allocInfo, m_descriptorSets.data()), "Failed to allocate descriptor sets");

	// Configure descriptor sets.
	for (size_t i = 0; i < m_vkManager.NumSwapChainImages(); ++i)
	{
		// Specify which buffer we want the descriptor to refer to.
		VkDescriptorBufferInfo bufferInfo{};
		{
			bufferInfo.buffer = m_uniformBuffers[i];
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
		vkUpdateDescriptorSets(m_vkManager.GetDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}

uint32_t HelloTriangle::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	// Query the types of memory

	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(m_vkManager.GetPhysicalDevice(), &memoryProperties);

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
		allocInfo.commandPool = m_commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;	// Submit to queue for execution but cannot be called from other command buffers (secondary).
		allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();

		VK_ASSERT(vkAllocateCommandBuffers(m_vkManager.GetDevice(), &allocInfo, m_commandBuffers.data()), "Failed to allocate command buffers");
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
			renderPassInfo.renderPass = m_renderPass;
			renderPassInfo.framebuffer = m_swapChainFrameBuffers[i];
			renderPassInfo.renderArea.offset = {0, 0};
			renderPassInfo.renderArea.extent = m_vkManager.GetSwapChainExtent();

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
		allocInfo.commandPool = m_commandPool;
		allocInfo.commandBufferCount = 1;
	}

	VkCommandBuffer commandBuffer;
	VK_ASSERT(vkAllocateCommandBuffers(m_vkManager.GetDevice(), &allocInfo, &commandBuffer), "Failed to allocate command buffers");

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

	VK_ASSERT(vkQueueSubmit(m_vkManager.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit queue");

	// Wait for the transfer to finish.
	VK_ASSERT(vkQueueWaitIdle(m_vkManager.GetGraphicsQueue()), "Failed to wait");

	vkFreeCommandBuffers(m_vkManager.GetDevice(), m_commandPool, 1, &commandBuffer);
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
	VK_ASSERT(vkCreateImage(m_vkManager.GetDevice(), &imageInfo, nullptr, &image), "Failed to create image");

	VkMemoryRequirements memoryRequirements{};
	vkGetImageMemoryRequirements(m_vkManager.GetDevice(), image, &memoryRequirements);

	VkMemoryAllocateInfo allocInfo{};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memoryRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, properties);
	}

	VK_ASSERT(vkAllocateMemory(m_vkManager.GetDevice(), &allocInfo, nullptr, &imageMemory), "Failed to allocate image memory");

	vkBindImageMemory(m_vkManager.GetDevice(), image, imageMemory, 0);
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
		VK_ASSERT(vkMapMemory(m_vkManager.GetDevice(), stagingBufferMemory, 0, imageSize, 0, &data), "Failed to map buffer");
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(m_vkManager.GetDevice(), stagingBufferMemory);
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

	vkDestroyBuffer(m_vkManager.GetDevice(), stagingBuffer, nullptr);
	vkFreeMemory(m_vkManager.GetDevice()	, stagingBufferMemory, nullptr);
	
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
	m_textureImageView = m_vkManager.CreateImageView(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, m_mipLevels);
}

void HelloTriangle::CreateTextureSampler()
{
	// Textures should be accessed through a sampler.
	// Samplers will apply filtering and transformations.

	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(m_vkManager.GetPhysicalDevice(), &properties);
	
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
	VK_ASSERT(vkCreateSampler(m_vkManager.GetDevice(), &samplerInfo, nullptr, &m_textureSampler), "Couldn't create texture sampler");
}

void HelloTriangle::GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
	// Check if format supports linear blitting. Not all platforms support this.
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(m_vkManager.GetPhysicalDevice(), imageFormat, &formatProperties);

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

	CreateImage(	m_vkManager.GetSwapChainExtent().width, 
	                m_vkManager.GetSwapChainExtent().height,
	                1,
	                m_vkManager.GetMSAASamples(), 
	                depthFormat,
	                VK_IMAGE_TILING_OPTIMAL, 
	                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
	                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depthImage, m_depthImageMemory);

	m_depthImageView = m_vkManager.CreateImageView(m_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

	TransitionImageLayout(m_depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
}

VkFormat HelloTriangle::FindDepthFormat()
{
	return FindSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

bool HelloTriangle::HasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void HelloTriangle::CreateColorResources()
{
	// Create multisampled color buffer

	VkFormat colorFormat = m_vkManager.GetSwapChainImageFormat();

	CreateImage(
		m_vkManager.GetSwapChainExtent().width, 
		m_vkManager.GetSwapChainExtent().height,
		1, m_vkManager.GetMSAASamples(), 
		colorFormat, 
		VK_IMAGE_TILING_OPTIMAL, 
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
		m_colorImage, 
		m_colorImageMemory);

	m_colorImageView = m_vkManager.CreateImageView(m_colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}

void HelloTriangle::CleanupSwapChain()
{
	m_imguiManager.Cleanup();
	
	vkDestroyImageView(m_vkManager.GetDevice(), m_colorImageView, nullptr);
	vkDestroyImage(m_vkManager.GetDevice(), m_colorImage, nullptr);
	vkFreeMemory(m_vkManager.GetDevice(), m_colorImageMemory, nullptr);
	
	vkDestroyImageView(m_vkManager.GetDevice(), m_depthImageView, nullptr);
	vkDestroyImage(m_vkManager.GetDevice(), m_depthImage, nullptr);
	vkFreeMemory(m_vkManager.GetDevice(), m_depthImageMemory, nullptr);
	
	for (auto& framebuffer : m_swapChainFrameBuffers)
	{
		vkDestroyFramebuffer(m_vkManager.GetDevice(), framebuffer, nullptr);
	}

	vkFreeCommandBuffers(m_vkManager.GetDevice(), m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());

	vkDestroyPipeline(m_vkManager.GetDevice(), m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_vkManager.GetDevice(), m_pipelineLayout, nullptr);
	vkDestroyRenderPass(m_vkManager.GetDevice(), m_renderPass, nullptr);

	for (auto& imageView : m_vkManager.GetSwapChainImageViews())
	{
		vkDestroyImageView(m_vkManager.GetDevice(), imageView, nullptr);
	}

	vkDestroySwapchainKHR(m_vkManager.GetDevice(), m_vkManager.GetSwapChain(), nullptr);

	for (size_t i = 0; i < m_vkManager.GetSwapChainImages().size(); ++i)
	{
		vkDestroyBuffer(m_vkManager.GetDevice(), m_uniformBuffers[i], nullptr);
		vkFreeMemory(m_vkManager.GetDevice(), m_uniformBuffersMemory[i], nullptr);
	}

	vkDestroyDescriptorPool(m_vkManager.GetDevice(), m_descriptorPool, nullptr);
}

void HelloTriangle::Cleanup()
{
	CleanupSwapChain();

	vkDestroySampler(m_vkManager.GetDevice(), m_textureSampler, nullptr);
	vkDestroyImageView(m_vkManager.GetDevice(), m_textureImageView, nullptr);
	vkDestroyImage(m_vkManager.GetDevice(), m_textureImage, nullptr);
	vkFreeMemory(m_vkManager.GetDevice(), m_textureImageMemory, nullptr);

	vkDestroyDescriptorSetLayout(m_vkManager.GetDevice(), m_descriptorSetLayout, nullptr);
	
	vkDestroyBuffer(m_vkManager.GetDevice(), m_vertexBuffer, nullptr);
	vkFreeMemory(m_vkManager.GetDevice(), m_vertexBufferMemory, nullptr);

	vkDestroyBuffer(m_vkManager.GetDevice(), m_indexBuffer, nullptr);
	vkFreeMemory(m_vkManager.GetDevice(), m_indexBufferMemory, nullptr);
	
	for(auto i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		vkDestroySemaphore(m_vkManager.GetDevice(), m_vkManager.GetImageAvailableSemaphores()[i], nullptr);
		vkDestroySemaphore(m_vkManager.GetDevice(), m_vkManager.GetRenderFinishedSemaphores()[i], nullptr);
		vkDestroyFence(m_vkManager.GetDevice(), m_vkManager.GetInFlightFences()[i], nullptr);
	}
	
	vkDestroyCommandPool(m_vkManager.GetDevice(), m_commandPool, nullptr);
	
	// Device queues (graphics queue) are implicitly destroyed when the device is destroyed.
	vkDestroyDevice(m_vkManager.GetDevice(), nullptr);
	
	if (g_enableValidationLayers)
	{
		DebugLayer::DestroyDebugUtilsMessengerEXT(m_vkManager.GetInstance(), m_vkManager.GetDebugMessenger(), nullptr);
	}

	vkDestroySurfaceKHR(m_vkManager.GetInstance(), m_vkManager.GetSurface(), nullptr);
	
	// Don't destroy physical device since it is destroyed implicitly when the instance is destroyed.
	vkDestroyInstance(m_vkManager.GetInstance(), nullptr);

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
