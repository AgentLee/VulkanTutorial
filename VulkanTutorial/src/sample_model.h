#pragma once

#include "vulkan_base.h"
//#include "texture.h"

class SampleModel : public VulkanBase
{
public:
	void Initialize() override;
	void SubmitDrawCall(uint32_t imageIndex) override;
	void Cleanup() override;
	
	void CreateRenderPass() override;
	void CreateGraphicsPipeline() override;
	void CreateDescriptorSetLayout() override;
	void CreateDescriptorSet() override;
	void CreateDescriptorPool() override;
	void CreateCommandPool() override;
	void CreateFrameBuffers() override;
	void CreateCommandBuffers() override;

	void CreateTextureImage();
	void CreateTextureImageView();
	void CreateTextureSampler();
	
	void UpdateUniformBuffers(uint32_t currentImage);

	// Copying new data each frame so no staging buffer.
	// Multiple buffers make sense since multiple frames can be in flight at the same time.
	std::vector<VkBuffer> m_uniformBuffers;
	std::vector<VkDeviceMemory> m_uniformBuffersMemory;

	VkImage m_textureImage;
	VkDeviceMemory m_textureImageMemory;
	VkImageView m_textureImageView;
	VkSampler m_textureSampler;

	uint32_t m_mipLevels;

	//Mesh m_mesh;
};