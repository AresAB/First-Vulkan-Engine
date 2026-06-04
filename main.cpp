#define VOLK_IMPLEMENTATION
#include <vulkan/vulkan.h>
#include <volk/volk.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <array>
#include <string>
#include <iostream>
#include <filesystem>
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "slang/slang.h"
#include "slang/slang-com-ptr.h"
#include <ktx.h>
#include <ktxvulkan.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <cstdlib>

static inline void chk(VkResult result) {
	if (result != VK_SUCCESS) {
		std::cerr << "Vulkan call returned an error (" << result << ")\n";
		exit(result);
	}
}
static inline void chk(bool result) {
	if (!result) {
		std::cerr << "Call returned an error\n";
		exit(result);
	}
}

int main(int argc, char* argv[])
{
	chk(SDL_Init(SDL_INIT_VIDEO));
	chk(SDL_Vulkan_LoadLibrary(NULL));
	chk(volkInitialize());

	// Create our Vulkan Instance
	// -------------------------------
	// declare application and vulkan instance information
	VkApplicationInfo app_info{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "My First Vulkan Engine",
		.apiVersion = VK_API_VERSION_1_3
	};

	// get extensions from Vulkan needed for OS and SDL compatability
	uint32_t num_extensions = 0;
	// array of extension names
	char const* const* pp_extensions = SDL_Vulkan_GetInstanceExtensions(&num_extensions);
	
	// here is where we would add enabled layers as well
	// - apparently you can enable layers through the configurator
	//   instead, which is what I'll be trying to do
	VkInstanceCreateInfo vk_instance_info{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &app_info,
		.enabledExtensionCount = num_extensions,
		.ppEnabledExtensionNames = pp_extensions
	};
	VkInstance vk_instance{ VK_NULL_HANDLE };
	chk(vkCreateInstance(&vk_instance_info, nullptr, &vk_instance));
	volkLoadInstance(vk_instance);
	// -------------------------------
	
	// Get our physical device
	// - - - - - - - - - - - - - - - -
	uint32_t num_physical_devices = 0;
	// this form of vkEnumerate will fill num_physical_devices
	chk(vkEnumeratePhysicalDevices(vk_instance, &num_physical_devices, NULL));
	VkPhysicalDevice* physical_devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * num_physical_devices);
	// this form of vkEnumerate will fill physical_devices up to num
	chk(vkEnumeratePhysicalDevices(vk_instance, &num_physical_devices, physical_devices));

	// If user chose a GPU to use via command line argument,
	// use that GPU, otherwise use any GPU (w/ priority to discrete)
	// Throw error if no GPUs support Vulkan 1.3
	uint32_t physical_device_i = num_physical_devices;
	if(argc > 1) {
		physical_device_i = std::stoi(argv[1]);
		assert(physical_device_i < num_physical_devices);
	}
	else {
		std::cout << "GPU(s)\n------------------------\n";
		for(uint32_t i = 0; i < num_physical_devices; i++) {
			VkPhysicalDeviceProperties2 physical_device_prop{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
			vkGetPhysicalDeviceProperties2(physical_devices[i], &physical_device_prop);
			std::cout << "  |- (" << i << ") " << physical_device_prop.properties.deviceName << "\n";
			if(VK_API_VERSION_MAJOR(physical_device_prop.properties.apiVersion) >= 1 && (VK_API_VERSION_MINOR(physical_device_prop.properties.apiVersion) >= 3 || VK_API_VERSION_MAJOR(physical_device_prop.properties.apiVersion) > 1) && (physical_device_prop.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU || i < physical_device_i)) {
				physical_device_i = i;
			}
		}
		std::cout << "------------------------\n";
		if(physical_device_i == num_physical_devices) {
			std::cerr << "ERROR: No GPU found that supports Vulkan 1.3 or above\n";
		}
	}

	VkPhysicalDevice physical_device = physical_devices[physical_device_i];
	free(physical_devices);
	VkPhysicalDeviceProperties2 physical_device_prop{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
	vkGetPhysicalDeviceProperties2(physical_device, &physical_device_prop);

	std::cout << "Selected GPU: (" << physical_device_i << ") " << physical_device_prop.properties.deviceName << "\nSupports up to Vulkan API " << VK_API_VERSION_MAJOR(physical_device_prop.properties.apiVersion) << "." << VK_API_VERSION_MINOR(physical_device_prop.properties.apiVersion) << "\nVulkan Physical Device Type " << physical_device_prop.properties.deviceType << "\n";
	// - - - - - - - - - - - - - - - -
}
