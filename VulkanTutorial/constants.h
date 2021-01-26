#include <iostream>
#include <glm/glm.hpp>

// Screen coordinates
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const int MAX_FRAMES_IN_FLIGHT = 2;

const std::string MODEL_PATH = "meshes/viking_room.obj";
const std::string TEXTURE_PATH = "textures/viking_room.png";

const std::vector<const char*> g_validationLayers = {
	"VK_LAYER_KHRONOS_validation",
};

const std::vector<const char*> g_deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

#ifdef NDEBUG
const bool g_enableValidationLayers = false;
#else
const bool g_enableValidationLayers = true;
#endif

#define SELECT_FIRST_DEVICE
#ifdef SELECT_FIRST_DEVICE
#define SELECT_FIRST_DEVICE
#else
#define SELECT_BEST_DEVICE
#endif

struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};