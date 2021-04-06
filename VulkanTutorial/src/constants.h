#pragma once

#include <iostream>
#include <glm/glm.hpp>

// Screen coordinates
static const uint32_t WIDTH = 800;
static const uint32_t HEIGHT = 600;

static const int MAX_FRAMES_IN_FLIGHT = 2;

static const std::string MODEL_PATH = "meshes/wahoo.obj";
static const std::string TEXTURE_PATH = "textures/wahoo.bmp";

static const std::string SHADER_DIRECTORY = "src/shaders/";

const std::vector<static const char*> g_validationLayers = {
	"VK_LAYER_KHRONOS_validation",
};

const std::vector<static const char*> g_deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

#ifdef NDEBUG
static const bool g_enableValidationLayers = false;
#else
static const bool g_enableValidationLayers = true;
#endif

#define SELECT_FIRST_DEVICE
#ifdef SELECT_FIRST_DEVICE
#define SELECT_FIRST_DEVICE
#else
#define SELECT_BEST_DEVICE
#endif

static size_t g_currentFrame = 0;
