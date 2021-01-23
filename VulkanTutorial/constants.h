#include <iostream>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> g_validationLayers = {
	"VK_LAYER_KHRONOS_validation",
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
