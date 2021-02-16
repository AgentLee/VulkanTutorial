#pragma once

#include <vulkan/vulkan.h>

static void CreateAttachmentDescription(
	VkAttachmentDescription& attachment,
	VkFormat format,
	VkAttachmentDescriptionFlags flags,
	VkSampleCountFlagBits sampleCountFlags, 
	VkAttachmentLoadOp loadOp, 
	VkAttachmentStoreOp storeOp, 
	VkAttachmentLoadOp stencilLoadOp, 
	VkAttachmentStoreOp stencilStoreOp,
	VkImageLayout initalLayout,
	VkImageLayout finalLayout)
{
	attachment.format = format;
	attachment.flags = flags;
	attachment.samples = sampleCountFlags;

	// What to do with the attachment data before/after rendering.
	attachment.loadOp = loadOp;
	attachment.storeOp = storeOp;

	// Same as above except for the stencil data.
	attachment.stencilLoadOp = stencilLoadOp;
	attachment.stencilStoreOp = stencilStoreOp;

	// Initial - Which layout the image will have before the pass begins.
	// Final - Specifies layout to automatically transition to when the render pass finishes.
	attachment.initialLayout = initalLayout;		// We don't care about the previous image since we clear it anyway.
	attachment.finalLayout = finalLayout;			// The image should be ready to present in the swap chain.
}

static void CreateAttachmentReference(VkAttachmentReference& attachmentReference, uint32_t attachment, VkImageLayout layout)
{
	attachmentReference.attachment = attachment;	// Which attachment to reference by index.
	attachmentReference.layout = layout;			// Which layout we want the attachment to have.
}

static void CreateSubpassDescription(
	VkSubpassDescription& subpass,
	VkSubpassDescriptionFlags flags,
	VkPipelineBindPoint pipelineBindPoint,
	uint32_t inputAttachmentCount,
	const VkAttachmentReference* inputAttachments,
	uint32_t colorAttachmentCount, 
	const VkAttachmentReference* colorAttachments,
	const VkAttachmentReference* resolveAttachments,
	const VkAttachmentReference* depthAttachments,
	uint32_t preserveAttachmentCount,
	const uint32_t* preserveAttachments)
{
	subpass.flags = flags;
	subpass.pipelineBindPoint = pipelineBindPoint;
	subpass.inputAttachmentCount = inputAttachmentCount;
	subpass.pInputAttachments = inputAttachments;
	subpass.colorAttachmentCount = colorAttachmentCount;
	subpass.pColorAttachments = colorAttachments;
	subpass.pResolveAttachments = resolveAttachments;
	subpass.pDepthStencilAttachment = depthAttachments;
	subpass.preserveAttachmentCount = preserveAttachmentCount;
	subpass.pPreserveAttachments = preserveAttachments;
}

static void CreateSubpassDependency(VkSubpassDependency& dependency, VkDependencyFlags flags, uint32_t srcSubpass, uint32_t dstSubpass, VkPipelineStageFlags srcStageMask, VkAccessFlags srcAccessMask, VkPipelineStageFlags dstStageMask, VkAccessFlags dstAccessMask)
{
	// Use a dependency to make sure the render pass waits until the pipeline writes to the buffer.
	// Prevent the transition from happening until it's ready.

	dependency.dependencyFlags = flags;

	// Dependency indices and subpass
	{
		dependency.srcSubpass = srcSubpass;
		dependency.dstSubpass = dstSubpass;
	}

	// Specify which operations to wait on and which stages they should occur
	// Wait for swap chain to finish reading from image before we can access it.
	// Make sure we specify the stages to prevent conflicts between transitions (depth vs color)
	{
		dependency.srcStageMask = srcStageMask;
		dependency.srcAccessMask = srcAccessMask;
	}

	{
		dependency.dstStageMask = dstStageMask;
		dependency.dstAccessMask = dstAccessMask;
	}
}

static void VKCreateDescriptorPool(VkDevice device, VkDescriptorPool* descriptorPool, VkDescriptorPoolSize* poolSizes, uint32_t poolSizeCount, uint32_t maxSets)
{
	VkDescriptorPoolCreateInfo poolInfo{};
	{
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = poolSizeCount;
		poolInfo.pPoolSizes = poolSizes;
		poolInfo.maxSets = maxSets;
	}

	// Create descriptor pool
	//VK_ASSERT(vkCreateDescriptorPool(device, &poolInfo, nullptr, descriptorPool), "Failed to create descriptor pool");
	vkCreateDescriptorPool(device, &poolInfo, nullptr, descriptorPool);
}

//https://frguthmann.github.io/posts/vulkan_imgui/
static void VKCreateCommandBuffers(VkDevice device, VkCommandBuffer* commandBuffer, uint32_t commandBufferCount, VkCommandPool &commandPool) {
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.commandBufferCount = commandBufferCount;
	vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, commandBuffer);
}

static VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(VulkanManager::GetVulkanManager().GetPhysicalDevice(), format, &properties);

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

static VkFormat FindDepthFormat()
{
	return FindSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}