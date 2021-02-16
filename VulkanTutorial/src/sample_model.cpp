#include "sample_model.h"

#include <array>

#include "constants.h"
#include "helpers.h"
#include "vertex.h"

void SampleModel::Initialize()
{
}

void SampleModel::SubmitDrawCall(uint32_t imageIndex)
{
	UpdateUniformBuffers(imageIndex);
}

void SampleModel::Cleanup()
{
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
