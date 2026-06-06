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
	uint32_t physical_device_index = num_physical_devices;
	if(argc > 1) {
		physical_device_index = std::stoi(argv[1]);
		assert(physical_device_index < num_physical_devices);
	}
	else {
		std::cout << "GPU(s)\n------------------------\n";
		for(uint32_t i = 0; i < num_physical_devices; i++) {
			VkPhysicalDeviceProperties2 physical_device_prop{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
			vkGetPhysicalDeviceProperties2(physical_devices[i], &physical_device_prop);
			std::cout << "  |- (" << i << ") " << physical_device_prop.properties.deviceName << "\n";
			if(VK_API_VERSION_MAJOR(physical_device_prop.properties.apiVersion) >= 1 && (VK_API_VERSION_MINOR(physical_device_prop.properties.apiVersion) >= 3 || VK_API_VERSION_MAJOR(physical_device_prop.properties.apiVersion) > 1) && (physical_device_prop.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU || i < physical_device_index)) {
				physical_device_index = i;
			}
		}
		std::cout << "------------------------\n";
		if(physical_device_index == num_physical_devices) {
			std::cerr << "ERROR: No GPU found that supports Vulkan 1.3 or above\n";
		}
	}

	VkPhysicalDevice physical_device = physical_devices[physical_device_index];
	free(physical_devices);
	VkPhysicalDeviceProperties2 physical_device_prop{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
	vkGetPhysicalDeviceProperties2(physical_device, &physical_device_prop);

	std::cout << "Selected GPU: (" << physical_device_index << ") " << physical_device_prop.properties.deviceName << "\nSupports up to Vulkan API " << VK_API_VERSION_MAJOR(physical_device_prop.properties.apiVersion) << "." << VK_API_VERSION_MINOR(physical_device_prop.properties.apiVersion) << "\nVulkan Physical Device Type " << physical_device_prop.properties.deviceType << "\nIf you wish to use a different GPU, relaunch executable with GPU index as first parameter\n";
	if(VK_API_VERSION_MAJOR(physical_device_prop.properties.apiVersion) < 1 || (VK_API_VERSION_MAJOR(physical_device_prop.properties.apiVersion) == 1 && VK_API_VERSION_MINOR(physical_device_prop.properties.apiVersion) < 3)) std::cerr << "ERROR: selected GPU does not support up to Vulkan API 1.3\n";
	// - - - - - - - - - - - - - - - -
	
	// Queues
	// -------------------------------
	uint32_t num_queue_families = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &num_queue_families, nullptr);
	VkQueueFamilyProperties* queue_families_properties = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * num_queue_families);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &num_queue_families, queue_families_properties);
	uint32_t queue_family_index = 0;
	for(uint32_t i = 0; i < num_queue_families; i++) {
		// this is how we tell if it is a graphics queue
		if(queue_families_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			queue_family_index = i;
			break;
		}
	}
	free(queue_families_properties);

	// This function checks if queue can present to screen
	// Throw error if queue doesn't support presentation instead of
	// doing synchronization to try and fix this rare situation
	chk(SDL_Vulkan_GetPresentationSupport(vk_instance, physical_device, queue_family_index));
	
	// Queue create info will want an array of floats between 0 and 1
	// indicating priority level of each queue we create
	float max_queue_priority = 1.0f; // only 1 value since we want 1 queue
	VkDeviceQueueCreateInfo queue_info{
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = queue_family_index,
		.queueCount = 1,
		.pQueuePriorities = &max_queue_priority
	};
	// -------------------------------
	
	// Device & Queue Creation
	// - - - - - - - - - - - - - - - -
	// Here is where we get any features outside of the base API
	VkPhysicalDeviceVulkan12Features enabled_vk_1_2_features{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.descriptorIndexing = true,
		.shaderSampledImageArrayNonUniformIndexing = true,
		.descriptorBindingVariableDescriptorCount = true,
		.runtimeDescriptorArray = true,
		.bufferDeviceAddress = true
	};
	VkPhysicalDeviceVulkan13Features enabled_vk_1_3_features{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.pNext = &enabled_vk_1_2_features,
		.synchronization2 = true,
		.dynamicRendering = true
	};
	// Since this is the og features, here is where you would
	// enable stuff related to the actual graphics pipeline
	// structure, like geometry and tessellation shaders
	VkPhysicalDeviceFeatures enabled_vk_1_0_features{
		.samplerAnisotropy = VK_TRUE
	};

	// Only extension we want, would have to set up an array if more
	const char* extension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
	
	VkDeviceCreateInfo device_info{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &enabled_vk_1_3_features,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &queue_info,
		.enabledExtensionCount = 1,
		.ppEnabledExtensionNames = &extension,
		.pEnabledFeatures = &enabled_vk_1_0_features
	};
	VkDevice device{ VK_NULL_HANDLE };
	chk(vkCreateDevice(physical_device, &device_info, nullptr, &device));

	VkQueue queue{ VK_NULL_HANDLE };
	vkGetDeviceQueue(device, queue_family_index, 0, &queue);
	// - - - - - - - - - - - - - - - -

	// VMA Setup
	// -------------------------------
	// We have been setting of our allocator parameters to NULL
	// so far, which normally means they would just be using
	// malloc and free, but we would rather set up VMA and
	// let it handle it instead.
	
	// This wants pointers to Vulkan functions
	VmaVulkanFunctions vk_functions {
		.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
		.vkGetDeviceProcAddr = vkGetDeviceProcAddr,
		.vkCreateImage = vkCreateImage
	};
	VmaAllocatorCreateInfo allocator_info{
		.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice = physical_device,
		.device = device,
		.pVulkanFunctions = &vk_functions,
		.instance = vk_instance
	};
	VmaAllocator allocator{ VK_NULL_HANDLE };
	chk(vmaCreateAllocator(&allocator_info, &allocator));
	// -------------------------------
}
