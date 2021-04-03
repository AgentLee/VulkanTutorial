#pragma once

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#endif

class Texture
{
public:
	Texture(const char* path)
	{
		// Get image
		m_pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);

		//if (!m_pixels)
		//{
		//	ASSERT(false, "Failed to load texture image");
		//}
	}

	void Free()
	{
		// We loaded everything into data so we can now clean up pixels.
		stbi_image_free(m_pixels);
	}

	int width, height, channels;
	stbi_uc* m_pixels;
};