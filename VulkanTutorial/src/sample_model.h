#pragma once

#include "buffer.h"
#include "image.h"
#include "mesh.h"
#include "transform.h"
#include "vulkan_base.h"

class SampleModel : public VulkanBase
{
public:
	void Initialize() override;
	void Reinitialize() override;
	void SubmitDrawCall(uint32_t imageIndex, Camera&) override;
	void Cleanup(bool recreateSwapchain) override;
	
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
	
	void UpdateUniformBuffers(uint32_t currentImage, Camera& camera);

	void Translate()
	{
		m_transform.Translate(glm::vec3(0.001, 0, 0));
	}

	void Rotate(glm::vec3 axis, float angle)
	{
		m_transform.Rotate(axis, angle);
	}
	
	void CreateBuffers()
	{
		auto& commandPool = VulkanManager::GetVulkanManager().GetCommandPool();
		m_vertexBuffer = Buffer(m_mesh.m_vertices, commandPool, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		m_indexBuffer = Buffer(m_mesh.m_indices, commandPool, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	}

	void CreateUniformBuffers()
	{
		auto numSwapChainImages = VulkanManager::GetVulkanManager().NumSwapChainImages();
		m_uniformBuffers.resize(numSwapChainImages);

		VkDeviceSize bufferSize = sizeof(UniformBufferObject);

		for (size_t i = 0; i < numSwapChainImages; ++i)
		{
			VulkanManager::GetVulkanManager().CreateBuffer(bufferSize, 
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
				m_uniformBuffers[i].m_buffer, 
				m_uniformBuffers[i].m_memory);
		}
	}

	Image m_texture;
	VkSampler m_textureSampler;
	uint32_t m_mipLevels;

	// Buffers should be allocated in one go.
	Buffer m_vertexBuffer;
	Buffer m_indexBuffer;
	// Copying new data each frame so no staging buffer.
	// Multiple buffers make sense since multiple frames can be in flight at the same time.
	std::vector<Buffer> m_uniformBuffers;

	Mesh m_mesh;

	Transform m_transform;
	
	Image m_colorImage;
	Image m_depthImage;
};