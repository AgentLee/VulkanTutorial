#include "imgui_manager.h"

#include <array>

#include "constants.h"
#include "helpers.h"

void ImGuiManager::Initialize()
{
	
}

void ImGuiManager::SubmitDrawCall(uint32_t imageIndex)
{
	// Start the Dear ImGui frame
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::ShowDemoWindow();
	ImGui::Render();
	
	// Submit ImGui commands
	VK_ASSERT(vkResetCommandPool(VulkanManager::GetVulkanManager().GetDevice(), m_commandPool, 0), "");

	VkCommandBufferBeginInfo info = {};
	{
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	}
	VK_ASSERT(vkBeginCommandBuffer(m_commandBuffers[imageIndex], &info), "");

	VkClearValue clearValue;
	clearValue.color = { 0.45f, 0.55f, 0.60f, 1.00f };

	VkRenderPassBeginInfo renderpassinfo = {};
	renderpassinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderpassinfo.renderPass = m_renderPass;
	renderpassinfo.framebuffer = m_frameBuffers[imageIndex];
	renderpassinfo.renderArea.extent.width = VulkanManager::GetVulkanManager().GetSwapChainExtent().width;
	renderpassinfo.renderArea.extent.height = VulkanManager::GetVulkanManager().GetSwapChainExtent().height;
	renderpassinfo.clearValueCount = 1;
	renderpassinfo.pClearValues = &clearValue;
	vkCmdBeginRenderPass(m_commandBuffers[imageIndex], &renderpassinfo, VK_SUBPASS_CONTENTS_INLINE);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_commandBuffers[imageIndex]);

	vkCmdEndRenderPass(m_commandBuffers[imageIndex]);
	VK_ASSERT(vkEndCommandBuffer(m_commandBuffers[imageIndex]), "");
}

void ImGuiManager::Cleanup()
{
	for (auto& framebuffer : m_frameBuffers) {
		vkDestroyFramebuffer(VulkanManager::GetVulkanManager().GetDevice(), framebuffer, nullptr);
	}

	vkFreeCommandBuffers(VulkanManager::GetVulkanManager().GetDevice(), m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());

	vkDestroyRenderPass(VulkanManager::GetVulkanManager().GetDevice(), m_renderPass, nullptr);
	vkDestroyCommandPool(VulkanManager::GetVulkanManager().GetDevice(), m_commandPool, nullptr);
	
	// Resources to destroy when the program ends
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	vkDestroyDescriptorPool(VulkanManager::GetVulkanManager().GetDevice(), m_descriptorPool, nullptr);
}

void ImGuiManager::CreateRenderPass()
{
	VkAttachmentDescription imguiAttachment{};
	CreateAttachmentDescription(
		imguiAttachment,
		VulkanManager::GetVulkanManager().GetSwapChainImageFormat(),
		0,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_LOAD,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	);

	VkAttachmentReference imguiAttachmentRef{};
	CreateAttachmentReference(imguiAttachmentRef, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	VkSubpassDescription subpass{};
	{
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &imguiAttachmentRef;
		subpass.pDepthStencilAttachment = nullptr;
		subpass.pResolveAttachments = nullptr;
	}

	VkSubpassDependency dependency{};
	CreateSubpassDependency(
		dependency,
		0,
		VK_SUBPASS_EXTERNAL,
		0,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	);

	std::array<VkAttachmentDescription, 1> attachments = {
		imguiAttachment
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

void ImGuiManager::CreateDescriptorSetLayout()
{
}

void ImGuiManager::CreateDescriptorPool()
{
	VkDescriptorPoolSize poolSizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VKCreateDescriptorPool(VulkanManager::GetVulkanManager().GetDevice(), &m_descriptorPool, poolSizes, 11, static_cast<uint32_t>(VulkanManager::GetVulkanManager().NumSwapChainImages()));
}

void ImGuiManager::CreateCommandPool()
{
	auto index = VulkanManager::GetVulkanManager().FindQueueFamilies(VulkanManager::GetVulkanManager().GetPhysicalDevice());

	// todo: move to new function
	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.queueFamilyIndex = index.graphicsFamily.value();
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(VulkanManager::GetVulkanManager().GetDevice(), &commandPoolCreateInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
		throw std::runtime_error("Could not create graphics command pool");
	}

	m_commandBuffers.resize(VulkanManager::GetVulkanManager().GetSwapChainImageViews().size());
	VKCreateCommandBuffers(VulkanManager::GetVulkanManager().GetDevice(), m_commandBuffers.data(), static_cast<uint32_t>(m_commandBuffers.size()), m_commandPool);
}

void ImGuiManager::CreateFrameBuffers()
{
	m_frameBuffers.resize(VulkanManager::GetVulkanManager().GetSwapChainImageViews().size());
	
	VkImageView attachment[1];
	VkFramebufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	info.renderPass = m_renderPass;
	info.attachmentCount = 1;
	info.pAttachments = attachment;
	info.width = VulkanManager::GetVulkanManager().GetSwapChainExtent().width;
	info.height = VulkanManager::GetVulkanManager().GetSwapChainExtent().height;
	info.layers = 1;
	for (uint32_t i = 0; i < m_frameBuffers.size(); ++i)
	{
		attachment[0] = VulkanManager::GetVulkanManager().GetSwapChainImageViews()[i];
		vkCreateFramebuffer(VulkanManager::GetVulkanManager().GetDevice(), &info, nullptr, &m_frameBuffers[i]);
	}
}
