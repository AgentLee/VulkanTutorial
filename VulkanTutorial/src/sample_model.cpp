#include "sample_model.h"

#include <array>

#include "helpers.h"
#include "constants.h"
#include "texture.h"

void SampleModel::Initialize()
{
	CreateTextureImage();
	CreateTextureImageView();
	CreateTextureSampler();

	//m_mesh = Mesh(MODEL_PATH.c_str());
}

void SampleModel::SubmitDrawCall(uint32_t imageIndex)
{
	UpdateUniformBuffers(imageIndex);
}

void SampleModel::Cleanup()
{
	// Cleanup swapchain
	{
		vkDestroyPipeline(VulkanManager::GetVulkanManager().GetDevice(), m_graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(VulkanManager::GetVulkanManager().GetDevice(), m_pipelineLayout, nullptr);
		vkDestroyRenderPass(VulkanManager::GetVulkanManager().GetDevice(), m_renderPass, nullptr);

		for (size_t i = 0; i < VulkanManager::GetVulkanManager().GetSwapChainImages().size(); ++i)
		{
			vkDestroyBuffer(VulkanManager::GetVulkanManager().GetDevice(), m_uniformBuffers[i], nullptr);
			vkFreeMemory(VulkanManager::GetVulkanManager().GetDevice(), m_uniformBuffersMemory[i], nullptr);
		}

		vkDestroyDescriptorPool(VulkanManager::GetVulkanManager().GetDevice(), m_descriptorPool, nullptr);
	}
	
	// Cleanup the rest
}

void SampleModel::CreateRenderPass()
{
	VkAttachmentDescription colorAttachment{};
	CreateAttachmentDescription(
		colorAttachment,
		VulkanManager::GetVulkanManager().GetSwapChainImageFormat(),
		0,
		VulkanManager::GetVulkanManager().GetMSAASamples(), 
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
		VulkanManager::GetVulkanManager().GetMSAASamples(),
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
		VulkanManager::GetVulkanManager().GetSwapChainImageFormat(),
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

	VK_ASSERT(vkCreateRenderPass(VulkanManager::GetVulkanManager().GetDevice(), &renderPassInfo, nullptr, &m_renderPass), "Failed to create render pass");
}

void SampleModel::CreateGraphicsPipeline()
{
	// Load shader code
	auto vsCode = ReadFile(SHADER_DIRECTORY + "vert.spv");
	auto fsCode = ReadFile(SHADER_DIRECTORY + "frag.spv");

	// Create modules
	auto vsModule = vkHelpers::CreateShaderModule(vsCode);
	auto fsModule = vkHelpers::CreateShaderModule(fsCode);

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
		pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
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
			pipelineInfo.renderPass = m_renderPass;
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

void SampleModel::CreateDescriptorSetLayout()
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

	VK_ASSERT(vkCreateDescriptorSetLayout(VulkanManager::GetVulkanManager().GetDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout), "Failed to create descriptor set layout");
}

// TODO:
// Textures need to be bound to this pass.
void SampleModel::CreateDescriptorSet()
{
	std::vector<VkDescriptorSetLayout> layouts(VulkanManager::GetVulkanManager().GetSwapChainImages().size(), GetDescriptorSetLayout());

	VkDescriptorSetAllocateInfo allocInfo{};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = GetDescriptorPool();
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
		vkUpdateDescriptorSets(VulkanManager::GetVulkanManager().GetDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}

void SampleModel::CreateDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	{
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = static_cast<uint32_t>(VulkanManager::GetVulkanManager().GetSwapChainImages().size());
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = static_cast<uint32_t>(VulkanManager::GetVulkanManager().GetSwapChainImages().size());
	}

	// Allocate one descriptor every frame.
	VKCreateDescriptorPool(VulkanManager::GetVulkanManager().GetDevice(), &m_descriptorPool, poolSizes.data(), static_cast<uint32_t>(poolSizes.size()), static_cast<uint32_t>(VulkanManager::GetVulkanManager().NumSwapChainImages()));
}

void SampleModel::CreateCommandPool()
{
	auto queueFamilyIndices = VulkanManager::GetVulkanManager().FindQueueFamilies(VulkanManager::GetVulkanManager().GetPhysicalDevice());

	VkCommandPoolCreateInfo poolInfo{};
	{
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();	// We want to draw, use the graphics family
		poolInfo.flags = 0;
	}

	VK_ASSERT(vkCreateCommandPool(VulkanManager::GetVulkanManager().GetDevice(), &poolInfo, nullptr, &m_commandPool), "Failed to create command pool");
}

void SampleModel::CreateFrameBuffers()
{
	
}

void SampleModel::CreateCommandBuffers()
{
	auto& vkManager = VulkanManager::GetVulkanManager();
	
	m_commandBuffers.resize(vkManager.GetSwapChainFrameBuffers().size());

	std::array<VkClearValue, 2> clearValues{};
	{
		clearValues[0].color = { 0, 0, 0, 1 };
		clearValues[1].depthStencil = { 1, 0 };
	}

	VkCommandBufferAllocateInfo allocInfo{};
	{
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;	// Submit to queue for execution but cannot be called from other command buffers (secondary).
		allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();

		VK_ASSERT(vkAllocateCommandBuffers(VulkanManager::GetVulkanManager().GetDevice(), &allocInfo, m_commandBuffers.data()), "Failed to allocate command buffers");
	}

	for (auto i = 0; i < m_commandBuffers.size(); ++i)
	{
		// TODO finish
	}
}

void SampleModel::CreateTextureImage()
{
	Texture texture(TEXTURE_PATH.c_str());

	m_mipLevels = vkHelpers::CaclulateMipLevels(texture.width, texture.height);

	VkDeviceSize imageSize = texture.width * texture.height * 4;	// Multiply by 4 for each channel RGBA

	Buffer stagingBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// Map to temp buffer
	stagingBuffer.Map(VulkanManager::GetVulkanManager().GetDevice(), texture.m_pixels);

	texture.Free();

	// With mipmapping enabled, source has to also be VK_IMAGE_USAGE_TRANSFER_SRC_BIT.
	auto usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	// Create the image
	VulkanManager::GetVulkanManager().CreateImage(texture.width,
		texture.height,
		m_mipLevels,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		usage,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_textureImage, m_textureImageMemory);

	// Copy the staging buffer to the image
	VulkanManager::GetVulkanManager().TransitionImageLayout(m_commandPool, m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_mipLevels);
	VulkanManager::GetVulkanManager().CopyBufferToImage(m_commandPool, stagingBuffer.m_buffer, m_textureImage, texture.width, texture.height);

	vkDestroyBuffer(VulkanManager::GetVulkanManager().GetDevice(), stagingBuffer.m_buffer, nullptr);
	vkFreeMemory(VulkanManager::GetVulkanManager().GetDevice(), stagingBuffer.m_memory, nullptr);

	// Generating mipmaps transitions the layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
	VulkanManager::GetVulkanManager().GenerateMipMaps(m_commandPool, m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, texture.width, texture.height, m_mipLevels);
}

void SampleModel::CreateTextureImageView()
{
	m_textureImageView = VulkanManager::GetVulkanManager().CreateImageView(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, m_mipLevels);
}

void SampleModel::CreateTextureSampler()
{
	VulkanManager::GetVulkanManager().CreateTextureSampler(m_textureSampler, static_cast<float>(m_mipLevels));
}

void SampleModel::UpdateUniformBuffers(uint32_t currentImage)
{
	float time = GetCurrentTime();
	time = 1.0f;

	UniformBufferObject ubo{};
	{
		ubo.model = glm::rotate(glm::mat4(1), time * glm::radians(0.0f), glm::vec3(0, 0, 1));
		ubo.view = glm::lookAt(glm::vec3(2, 2, 2), glm::vec3(0), glm::vec3(0, 0, 1));
		ubo.proj = glm::perspective(glm::radians(45.0f), (float)VulkanManager::GetVulkanManager().GetSwapChainExtent().width / (float)VulkanManager::GetVulkanManager().GetSwapChainExtent().height, 0.1f, 10.0f);
		ubo.proj[1][1] *= -1.0f;
	}

	void* data;
	VK_ASSERT(vkMapMemory(VulkanManager::GetVulkanManager().GetDevice(), m_uniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data), "Couldn't map uniform buffer");
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(VulkanManager::GetVulkanManager().GetDevice(), m_uniformBuffersMemory[currentImage]);
}
