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
#include "model_loader.h"

#include <cstdlib>

static inline void chk(VkResult result, int line) {
	if (result != VK_SUCCESS) {
		std::cerr << "Vulkan call returned an error (" << result << ") at line " << line << "\n";
		exit(result);
	}
}
static inline void chk(bool result, int line) {
	if (!result) {
		std::cerr << "Call returned an error at line " << line << "\n";
		exit(result);
	}
}
static inline void chk_swapchain(VkResult result, bool* update_swapchain, int line) {
	if(result < VK_SUCCESS) {
		if(result == VK_ERROR_OUT_OF_DATE_KHR) {
			*update_swapchain = true;
		}
		std::cerr << "Vulkan call returned an error (" << result << ") at " << line << "\n";
		exit(result);
	}
}

struct ShaderDataBuffer {
	VmaAllocation allocation{ VK_NULL_HANDLE };
	VmaAllocationInfo allocation_info{};
	VkBuffer buffer{ VK_NULL_HANDLE };
	VkDeviceAddress device_address{};
};
struct Texture {
	VmaAllocation allocation{ VK_NULL_HANDLE };
	VkImage image{ VK_NULL_HANDLE };
	VkImageView view{ VK_NULL_HANDLE };
	VkSampler sampler{ VK_NULL_HANDLE };
};
struct Engine {
	VkInstance instance{ VK_NULL_HANDLE };
	VkPhysicalDevice physical_device{ VK_NULL_HANDLE };
	VkDevice device{ VK_NULL_HANDLE };
	VkQueue queue{ VK_NULL_HANDLE };
	VmaAllocator allocator{ VK_NULL_HANDLE };
	SDL_Window* window;
	VkSurfaceKHR surface{ VK_NULL_HANDLE };
	VkSurfaceCapabilitiesKHR surface_caps;
	VkSwapchainCreateInfoKHR swapchainCI;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	uint32_t sc_image_count = 0;
	VkImage* sc_images;
	VkImageView* sc_image_views;
	VkFormat depth_format = VK_FORMAT_UNDEFINED;
	int window_width;
	int window_height;
	VkImage depth_image;
	VkImageCreateInfo depth_imageCI;
	VmaAllocation depth_image_allocation;
	VkImageView depth_image_view;
	uint32_t model_count;
	Model* models;
	uint16_t frame_count;
	uint32_t shader_count;
	ShaderDataBuffer** shader_data_buffers;
	VkCommandBuffer* command_buffers;
	VkFence* fences;
	VkSemaphore* image_acquired_semaphores;
	VkSemaphore* render_complete_semaphores;
	VkCommandPool command_pool{ VK_NULL_HANDLE };
	uint32_t texture_count;
	Texture* textures;
	VkDescriptorImageInfo* texture_descriptors;
	VkDescriptorSetLayout desc_set_layout_tex;
	VkDescriptorPool desc_pool;
	VkDescriptorSet desc_set;
	VkShaderModule* shader_modules;
	VkPipelineLayout pipeline_layout{ VK_NULL_HANDLE };
	VkPipeline* pipelines;
	bool closing = false;
	bool update_swapchain = false;
	uint32_t frame_index = 0;
	uint32_t image_index = 0;
	glm::vec3 og_cam_pos;
	glm::vec3 cam_pos;
	glm::mat4 og_cam_rot_mat;
	glm::mat4 cam_rot_mat;
	float cam_mv_spd;
	float cam_rot_spd;
	uint64_t last_time;
	uint64_t deltatime = 0;
};

struct EngineCreateInfo {
	int gpu_index = -1;
	VkFormat image_format = VK_FORMAT_B8G8R8A8_SRGB;
	VkColorSpaceKHR color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
	uint16_t frame_count = 2;
	glm::vec3 cam_pos{ 0.0f, 0.0f, -6.0f };
	glm::mat4 cam_rot_mat{ glm::mat4(1.0f) };
	float cam_mv_spd = 0.000005f;
	float cam_rot_spd = 0.005f;
	uint32_t texture_count;
	uint32_t model_count;
	uint32_t shader_count;
};

Engine create_engine(EngineCreateInfo engineCI) {
	chk(SDL_Init(SDL_INIT_VIDEO), __LINE__);
	chk(SDL_Vulkan_LoadLibrary(NULL), __LINE__);
	chk(volkInitialize(), __LINE__);

	Engine engine{};
	engine.frame_count = engineCI.frame_count;
	engine.og_cam_pos = engineCI.cam_pos;
	engine.cam_pos = engine.og_cam_pos;
	engine.og_cam_rot_mat = engineCI.cam_rot_mat;
	engine.cam_rot_mat = engine.og_cam_rot_mat;
	engine.cam_mv_spd = engineCI.cam_mv_spd;
	engine.cam_rot_spd = engineCI.cam_rot_spd;
	engine.model_count = engineCI.model_count;
	engine.shader_count = engineCI.shader_count;

	// Create our Vulkan Instance
	// -------------------------------
	// declare application and vulkan instance information
	VkApplicationInfo app_info{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "My First Vulkan Engine",
		.apiVersion = VK_API_VERSION_1_3
	};

	// get extensions from Vulkan needed for OS and SDL compatability
	uint32_t enabled_extension_count = 0;
	// array of extension names
	char const* const* pp_extensions = SDL_Vulkan_GetInstanceExtensions(&enabled_extension_count);
	
	// here is where we would add enabled layers as well
	// - apparently you can enable layers through the configurator
	//   instead, which is what I'll be trying to do
	VkInstanceCreateInfo vk_instance_info{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &app_info,
		.enabledExtensionCount = enabled_extension_count,
		.ppEnabledExtensionNames = pp_extensions
	};
	chk(vkCreateInstance(&vk_instance_info, nullptr, &engine.instance), __LINE__);
	volkLoadInstance(engine.instance);
	// -------------------------------
	
	// Get our physical device
	// - - - - - - - - - - - - - - - -
	uint32_t physical_device_count = 0;
	// this form of vkEnumerate will fill physical_device_count
	chk(vkEnumeratePhysicalDevices(engine.instance, &physical_device_count, NULL), __LINE__);
	VkPhysicalDevice* physical_devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * physical_device_count);
	// this form of vkEnumerate will fill physical_devices up to num
	chk(vkEnumeratePhysicalDevices(engine.instance, &physical_device_count, physical_devices), __LINE__);

	// Use any of the available GPUs (w/ priority to discrete)
	// Throw error if no GPUs support Vulkan 1.3
	// Override if user asks for a specific GPU via command line
	uint32_t physical_device_index = physical_device_count;

	std::cout << "GPU(s)\n------------------------\n";
	for(uint32_t i = 0; i < physical_device_count; i++) {
		VkPhysicalDeviceProperties2 physical_device_prop{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		vkGetPhysicalDeviceProperties2(physical_devices[i], &physical_device_prop);
		std::cout << "  |- (" << i << ") " << physical_device_prop.properties.deviceName << "\n";
		if(VK_API_VERSION_MAJOR(physical_device_prop.properties.apiVersion) >= 1 && (VK_API_VERSION_MINOR(physical_device_prop.properties.apiVersion) >= 3 || VK_API_VERSION_MAJOR(physical_device_prop.properties.apiVersion) > 1) && (physical_device_prop.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU || i < physical_device_index)) {
			physical_device_index = i;
		}
	}
	std::cout << "------------------------\n";

	if(engineCI.gpu_index != -1) {
		if(engineCI.gpu_index < physical_device_count) {
			physical_device_index = engineCI.gpu_index;
		}
		else { 
			std::cout << "ERROR: Input GPU index too high, defaulting back to automatic selection.\n";
		}
	}
	else if(physical_device_index == physical_device_count) {
		std::cerr << "ERROR: No GPU found that supports Vulkan 1.3 or above\n";
	}

	engine.physical_device = physical_devices[physical_device_index];
	free(physical_devices);
	VkPhysicalDeviceProperties2 physical_device_prop{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
	vkGetPhysicalDeviceProperties2(engine.physical_device, &physical_device_prop);

	std::cout << "Selected GPU: (" << physical_device_index << ") " << physical_device_prop.properties.deviceName << "\nSupports up to Vulkan API " << VK_API_VERSION_MAJOR(physical_device_prop.properties.apiVersion) << "." << VK_API_VERSION_MINOR(physical_device_prop.properties.apiVersion) << "\nVulkan Physical Device Type " << physical_device_prop.properties.deviceType << "\n\n";
	if(VK_API_VERSION_MAJOR(physical_device_prop.properties.apiVersion) < 1 || (VK_API_VERSION_MAJOR(physical_device_prop.properties.apiVersion) == 1 && VK_API_VERSION_MINOR(physical_device_prop.properties.apiVersion) < 3)) std::cerr << "ERROR: selected GPU does not support up to Vulkan API 1.3\n";
	// - - - - - - - - - - - - - - - -
	
	// Queues
	// -------------------------------
	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(engine.physical_device, &queue_family_count, nullptr);
	VkQueueFamilyProperties* queue_families_properties = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(engine.physical_device, &queue_family_count, queue_families_properties);
	uint32_t queue_family_index = 0;
	for(uint32_t i = 0; i < queue_family_count; i++) {
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
	chk(SDL_Vulkan_GetPresentationSupport(engine.instance, engine.physical_device, queue_family_index), __LINE__);
	
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
	chk(vkCreateDevice(engine.physical_device, &device_info, nullptr, &engine.device), __LINE__);

	vkGetDeviceQueue(engine.device, queue_family_index, 0, &engine.queue);
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
		.physicalDevice = engine.physical_device,
		.device = engine.device,
		.pVulkanFunctions = &vk_functions,
		.instance = engine.instance
	};
	chk(vmaCreateAllocator(&allocator_info, &engine.allocator), __LINE__);
	// -------------------------------

	// Window Setup
	// - - - - - - - - - - - - - - - -
	engine.window = SDL_CreateWindow("First Window", 1280u, 720u, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

	// We interact with the window through its "surface" in Vulkan
	chk(SDL_Vulkan_CreateSurface(engine.window, engine.instance, NULL, &engine.surface), __LINE__);

	chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(engine.physical_device, engine.surface, &engine.surface_caps), __LINE__);
	// - - - - - - - - - - - - - - - -

	// Swapchain Setup
	// -------------------------------

	// Default settings that will work on basically any machine
	// Real applications leave it up to user settings
 	engine.swapchainCI = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = engine.surface,
		.minImageCount = engine.surface_caps.minImageCount,
		.imageFormat = engineCI.image_format,
		.imageColorSpace = engineCI.color_space,
		.imageExtent{
			.width = engine.surface_caps.currentExtent.width,
			.height = engine.surface_caps.currentExtent.height
		},
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = engineCI.present_mode
	};
	chk(vkCreateSwapchainKHR(engine.device, &engine.swapchainCI, NULL, &engine.swapchain), __LINE__);

	chk(vkGetSwapchainImagesKHR(engine.device, engine.swapchain, &engine.sc_image_count, NULL), __LINE__);
	engine.sc_images = (VkImage*)malloc(engine.sc_image_count * sizeof(VkImage));
	chk(vkGetSwapchainImagesKHR(engine.device, engine.swapchain, &engine.sc_image_count, engine.sc_images), __LINE__);
	engine.sc_image_views = (VkImageView*)malloc(engine.sc_image_count * sizeof(VkImageView));
	for(uint32_t i = 0; i < engine.sc_image_count; i++) {
		VkImageViewCreateInfo viewCI {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = engine.sc_images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = engine.swapchainCI.imageFormat,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1
			}
		};
		chk(vkCreateImageView(engine.device, &viewCI, nullptr, &engine.sc_image_views[i]), __LINE__);
	}
	// -------------------------------

	// Depth Testing
	// - - - - - - - - - - - - - - - -
	
	// If you want things to properly render in 3d, you'll need
	// to setup depth testing.
	// First, we get our depth buffer format.
	VkFormat depth_format_list[2] = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	for(uint32_t i = 0; i < 2; i++) {
		VkFormatProperties2 format_props {
			.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2
		};
		vkGetPhysicalDeviceFormatProperties2(engine.physical_device, depth_format_list[i], &format_props);
		if(format_props.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			engine.depth_format = depth_format_list[i];
			break;
		}
	};

	// Then, we made our depth buffer (image).
	// Most of our options will be the default ones since
	// we effectively are just making a single empty image,
	// but we do need to tell it that it's a depth buffer (.usage).
	SDL_GetWindowSize(engine.window, &engine.window_width, &engine.window_height);
	engine.depth_imageCI = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = engine.depth_format,
		.extent {
			.width = (uint32_t)engine.window_width,
			.height = (uint32_t)engine.window_height,
			.depth = 1
		},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	// Images are pretty big and verbose when it comes to
	// memory management, let VMA handle it for us.
	// VMA_MEMORY_USAGE_AUTO automatically selects "best" usage mode,
	// and this flag just says to create some memory for image.
	VmaAllocationCreateInfo allocCI {
		.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};
	// Allocate memory, can destroy it later via vmaDestroyImage()
	chk(vmaCreateImage(engine.allocator, &engine.depth_imageCI, &allocCI, &engine.depth_image, &engine.depth_image_allocation, NULL), __LINE__);

	// Now we need to get out image view, which is what allows us
	// to actually interact with the image.
	// The subresourceRange specifies what we want to access from
	// the image, such as the "aspect" (color, stencil, depth, etc),
	// mipmap level, and layer level.
	VkImageViewCreateInfo depth_viewCI {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = engine.depth_image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = engine.depth_format,
		.subresourceRange {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.levelCount = 1,
			.layerCount = 1
		}
	};
	chk(vkCreateImageView(engine.device, &depth_viewCI, NULL, &engine.depth_image_view), __LINE__);
	// - - - - - - - - - - - - - - - -
	
	// GPU and CPU Parallelism
	// - - - - - - - - - - - - - - - -

	// Create multiple frame buffers to allow the CPU to work on
	// frames while GPU is processing previous ones (refered to
	// commonly as "frames in flight").
	// Stick to only 2-3 frame buffers to avoid high latency.
	engine.command_buffers = (VkCommandBuffer*)malloc(engine.frame_count * sizeof(VkCommandBuffer));
	engine.fences = (VkFence*)malloc(engine.frame_count * sizeof(VkFence));
	engine.image_acquired_semaphores = (VkSemaphore*)malloc(engine.frame_count * sizeof(VkSemaphore));
	// - - - - - - - - - - - - - - - -
	
	// Synchronization Objects
	// - - - - - - - - - - - - - - - -
	
	// We have to handle all of the synchronization between CPU
	// and GPU, and we have to make it safe (validation layers can
	// check that for us).
	// Fences signal work completion from GPU to CPU, Binary
	// Semaphores control what GPU has access to and can do,
	// Pipeline Barriers are like semaphores but for GPU queues.
	VkSemaphoreCreateInfo semaphoreCI {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};
	// We start our fences signaled via flags for application start
	VkFenceCreateInfo fenceCI {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};
	for(auto i = 0; i < engine.frame_count; i++) {
		chk(vkCreateFence(engine.device, &fenceCI, nullptr, &engine.fences[i]), __LINE__);
		chk(vkCreateSemaphore(engine.device, &semaphoreCI, nullptr, &engine.image_acquired_semaphores[i]), __LINE__);
	}
	engine.render_complete_semaphores = (VkSemaphore*)malloc(sizeof(VkSemaphore) * engine.sc_image_count);
	for(auto i = 0; i < engine.sc_image_count; i++) {
		chk(vkCreateSemaphore(engine.device, &semaphoreCI, nullptr, engine.render_complete_semaphores + i), __LINE__);
	}
	// - - - - - - - - - - - - - - - -

	// Command Buffers
	// -------------------------------a

	// We'll only be having one command pool, but they are cheap
	// so feel free to make more if needed.

	// Before recording buffers, they always have to be reset,
	// hency why we start it as resetted via flags.
	VkCommandPoolCreateInfo command_poolCI {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = queue_family_index
	};
	chk(vkCreateCommandPool(engine.device, &command_poolCI, nullptr, &engine.command_pool), __LINE__);

	VkCommandBufferAllocateInfo command_buffer_allocAI {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = engine.command_pool,
		.commandBufferCount = engine.frame_count
	};
	chk(vkAllocateCommandBuffers(engine.device, &command_buffer_allocAI, engine.command_buffers), __LINE__);
	// -------------------------------

	// Loading Textures
	// - - - - - - - - - - - - - - - -
	// Mostly moved outside of engine.cpp
	engine.texture_count = engineCI.texture_count;
	engine.textures = (Texture*)malloc(sizeof(Texture) * engine.texture_count);
	engine.texture_descriptors = (VkDescriptorImageInfo*)malloc(sizeof(VkDescriptorImageInfo) * engine.texture_count);

	// - - - - - - - - - - - - - - - -

	engine.models = (Model*)malloc(engine.model_count * sizeof(Model));
	engine.shader_data_buffers = (ShaderDataBuffer**)malloc(engine.shader_count * sizeof(ShaderDataBuffer*));
	engine.shader_modules = (VkShaderModule*)malloc(engine.shader_count * sizeof(VkShaderModule));
	engine.pipelines = (VkPipeline*)malloc(engine.shader_count * sizeof(VkPipeline));

	return engine;
}

void engine_recreate_swapchain(Engine* engine) {
	engine->update_swapchain = false;
	chk(vkDeviceWaitIdle(engine->device), __LINE__);
	chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(engine->physical_device, engine->surface, &engine->surface_caps), __LINE__);
	engine->swapchainCI.oldSwapchain = engine->swapchain;
	SDL_GetWindowSize(engine->window, &engine->window_width, &engine->window_height);
	engine->swapchainCI.imageExtent = {.width = (uint32_t)engine->window_width, .height = (uint32_t)engine->window_height };
	chk(vkCreateSwapchainKHR(engine->device, &engine->swapchainCI, nullptr, &engine->swapchain), __LINE__);
	for(auto i = 0; i < engine->sc_image_count; i++) {
		vkDestroyImageView(engine->device, engine->sc_image_views[i], nullptr);
		vkDestroySemaphore(engine->device, engine->render_complete_semaphores[i], nullptr);
	}
	chk(vkGetSwapchainImagesKHR(engine->device, engine->swapchain, &engine->sc_image_count, nullptr), __LINE__);
	engine->sc_images = (VkImage*)realloc(engine->sc_images, sizeof(VkImage) * engine->sc_image_count);
	chk(vkGetSwapchainImagesKHR(engine->device, engine->swapchain, &engine->sc_image_count, engine->sc_images), __LINE__);
	engine->sc_image_views = (VkImageView*)realloc(engine->sc_image_views, sizeof(VkImageView) * engine->sc_image_count);
	engine->render_complete_semaphores = (VkSemaphore*)realloc(engine->render_complete_semaphores, sizeof(VkSemaphore) * engine->sc_image_count);
	VkSemaphoreCreateInfo semaphoreCI {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};
	for(auto i = 0; i < engine->sc_image_count; i++) {
		VkImageViewCreateInfo viewCI {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = engine->sc_images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = engine->swapchainCI.imageFormat,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1
			}
		};
		chk(vkCreateImageView(engine->device, &viewCI, nullptr, &engine->sc_image_views[i]), __LINE__);
		chk(vkCreateSemaphore(engine->device, &semaphoreCI, nullptr, &engine->render_complete_semaphores[i]), __LINE__);
	}
	vkDestroySwapchainKHR(engine->device, engine->swapchainCI.oldSwapchain, nullptr);
	vmaDestroyImage(engine->allocator, engine->depth_image, engine->depth_image_allocation);
	vkDestroyImageView(engine->device, engine->depth_image_view, nullptr);
	engine->depth_imageCI.extent = {.width = (uint32_t)engine->window_width, .height = (uint32_t)engine->window_height, .depth = 1 };
	VmaAllocationCreateInfo allocCI {
		.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};
	chk(vmaCreateImage(engine->allocator, &engine->depth_imageCI, &allocCI, &engine->depth_image, &engine->depth_image_allocation, nullptr), __LINE__);
	VkImageViewCreateInfo viewCI {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = engine->depth_image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = engine->depth_format,
		.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1 }
	};
	chk(vkCreateImageView(engine->device, &viewCI, nullptr, &engine->depth_image_view), __LINE__);
}

void engine_load_texture_ktx(Engine* engine, uint32_t index, const char* filename){ 
	if(index >= engine->texture_count){
		std::cerr << "ERROR: Loading texture at index " << index << " when there are only " << engine->texture_count << " texture elements\n";
	}

	ktxTexture* ktx_texture = nullptr;
	ktxTexture_CreateFromNamedFile(filename, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_texture);
	// VK_IMAGE_USAGE_TRANSFER_DST_BIT tells it that we want
	// to transfer the image data from disk (our image file).
	VkImageCreateInfo tex_imgCI {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = ktxTexture_GetVkFormat(ktx_texture),
		.extent {
			.width = ktx_texture->baseWidth,
			.height = ktx_texture->baseHeight,
			.depth = 1
		},
		.mipLevels = ktx_texture->numLevels,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	VmaAllocationCreateInfo tex_image_allocCI{
		.usage = VMA_MEMORY_USAGE_AUTO
	};
	chk(vmaCreateImage(engine->allocator, &tex_imgCI, &tex_image_allocCI, &engine->textures[index].image, &engine->textures[index].allocation, nullptr), __LINE__);
	VkImageViewCreateInfo tex_viewCI {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = engine->textures[index].image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = tex_imgCI.format,
		.subresourceRange {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = ktx_texture->numLevels,
			.layerCount = 1
		}
	};
	chk(vkCreateImageView(engine->device, &tex_viewCI, nullptr, &engine->textures[index].view), __LINE__);

	// We can't just memcpy images unfortunately, we have to
	// upload the image data to a buffer and then issue a
	// command to copy the buffer into an image (while doing
	// necessary conversions for tiling and stuff).
	VkBuffer temp_image_buffer;
	VmaAllocation temp_image_allocation;
	VkBufferCreateInfo temp_image_bufferCI {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = (uint32_t)ktx_texture->dataSize,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
	};
	VmaAllocationCreateInfo temp_image_allocCI {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};
	VmaAllocationInfo temp_image_alloc_info;
	chk(vmaCreateBuffer(engine->allocator, &temp_image_bufferCI, &temp_image_allocCI, &temp_image_buffer, &temp_image_allocation, &temp_image_alloc_info), __LINE__);
	memcpy(temp_image_alloc_info.pMappedData, ktx_texture->pData, ktx_texture->dataSize);
	// Now we need to make our command buffers in order to
	// do the copy commands, and we'll want a fence to know
	// when it's done.
	VkFenceCreateInfo temp_fenceCI {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
	};
	VkFence temp_fence;
	chk(vkCreateFence(engine->device, &temp_fenceCI, nullptr, &temp_fence), __LINE__);
	VkCommandBuffer temp_cb;
	VkCommandBufferAllocateInfo temp_cbAI {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = engine->command_pool,
		.commandBufferCount = 1
	};
	chk(vkAllocateCommandBuffers(engine->device, &temp_cbAI, &temp_cb), __LINE__);
	// Now we need to record our copy command.
	// The layout (tiling) of the image in memory determines
	// what it can do, so we use vkCmdPipelineBarrier2 to
	// convert to a layout that makes mip levels transferable,
	// then copy all mip levels to our temp buffer via
	// vkCmdCopyBufferToImage, and then convert layout to
	// make mip levels readable from shaders.
	VkCommandBufferBeginInfo temp_cbBI {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	chk(vkBeginCommandBuffer(temp_cb, &temp_cbBI), __LINE__);
	VkImageMemoryBarrier2 barrier_tex_transfer {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.image = engine->textures[index].image,
		.subresourceRange {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = ktx_texture->numLevels,
			.layerCount = 1
		}
	};
	VkDependencyInfo barrier_tex_info {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrier_tex_transfer
	};
	vkCmdPipelineBarrier2(temp_cb, &barrier_tex_info);
	VkBufferImageCopy* copy_regions = (VkBufferImageCopy*)malloc(sizeof(VkBufferImageCopy) * ktx_texture->numLevels);
	for(auto j = 0; j < ktx_texture->numLevels; j++) {
		ktx_size_t mip_offset = 0;
		KTX_error_code ret = ktxTexture_GetImageOffset(ktx_texture, j, 0, 0, &mip_offset);
		copy_regions[j] = {
			.bufferOffset = mip_offset,
			.imageSubresource {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = (uint32_t)j,
				.layerCount = 1
			},
			.imageExtent {
				.width = ktx_texture->baseWidth >> j,
				.height = ktx_texture->baseHeight >> j,
				.depth = 1
			}
		};
	}
	vkCmdCopyBufferToImage(temp_cb, temp_image_buffer, engine->textures[index].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(ktx_texture->numLevels), copy_regions);
	VkImageMemoryBarrier2 barrier_tex_read {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
		.image = engine->textures[index].image,
		.subresourceRange {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = ktx_texture->numLevels,
			.layerCount = 1
		}
	};
	barrier_tex_info.pImageMemoryBarriers = &barrier_tex_read;
	vkCmdPipelineBarrier2(temp_cb, &barrier_tex_info);
	chk(vkEndCommandBuffer(temp_cb), __LINE__);
	VkSubmitInfo one_timeSI {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &temp_cb
	};
	chk(vkQueueSubmit(engine->queue, 1, &one_timeSI, temp_fence), __LINE__);
	chk(vkWaitForFences(engine->device, 1, &temp_fence, VK_TRUE, UINT64_MAX), __LINE__);
	free(copy_regions);
	vkDestroyFence(engine->device, temp_fence, nullptr);
	vmaDestroyBuffer(engine->allocator, temp_image_buffer, temp_image_allocation);
	// Wrap it up by defining the shader's sample behavior
	VkSamplerCreateInfo samplerCI {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.anisotropyEnable = VK_TRUE,
		.maxAnisotropy = 8.0f, // 8 is generally max
		.maxLod = (float)ktx_texture->numLevels
	};
	chk(vkCreateSampler(engine->device, &samplerCI, nullptr, &engine->textures[index].sampler), __LINE__);
	ktxTexture_Destroy(ktx_texture);
	engine->texture_descriptors[index] = {
		.sampler = engine->textures[index].sampler,
		.imageView = engine->textures[index].view,
		.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
	};
}

void engine_load_texture_descriptors(Engine* engine, VkShaderStageFlags shader_access_flags) {
	VkDescriptorBindingFlags desc_binding_flag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
	VkDescriptorSetLayoutBindingFlagsCreateInfo desc_set_binding_flagsCI {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.bindingCount = 1,
		.pBindingFlags = &desc_binding_flag
	};
	VkDescriptorSetLayoutBinding desc_set_layout_binding_tex{
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // we combine our texture images and samplers
		.descriptorCount = engine->texture_count,
		.stageFlags = shader_access_flags
	};
	VkDescriptorSetLayoutCreateInfo desc_set_layout_texCI {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = &desc_set_binding_flagsCI,
		.bindingCount = 1,
		.pBindings = &desc_set_layout_binding_tex
	};
	chk(vkCreateDescriptorSetLayout(engine->device, &desc_set_layout_texCI, nullptr, &engine->desc_set_layout_tex), __LINE__);

	VkDescriptorPoolSize desc_pool_size {
		.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = engine->texture_count
	};
	VkDescriptorPoolCreateInfo desc_poolCI {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 1,
		.poolSizeCount = 1,
		.pPoolSizes = &desc_pool_size
	};
	chk(vkCreateDescriptorPool(engine->device, &desc_poolCI, nullptr, &engine->desc_pool), __LINE__);

	// We are making 1 descriptor set, and enough descriptors for
	// each texture, as descriptor sets describe the interface, while
	// descriptors actually hold onto the data.
	VkDescriptorSetVariableDescriptorCountAllocateInfo var_desc_countAI {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
		.descriptorSetCount = 1,
		.pDescriptorCounts = &engine->texture_count
	};
	VkDescriptorSetAllocateInfo desc_setAI {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = &var_desc_countAI,
		.descriptorPool = engine->desc_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &engine->desc_set_layout_tex
	};
	chk(vkAllocateDescriptorSets(engine->device, &desc_setAI, &engine->desc_set), __LINE__);

	// We have allocated our descriptor set, now we need to fill it
	// with descriptors, which we set up in texture loading.
	VkWriteDescriptorSet write_desc_set {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = engine->desc_set,
		.dstBinding = 0,
		.descriptorCount = engine->texture_count,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = engine->texture_descriptors
	};
	vkUpdateDescriptorSets(engine->device, 1, &write_desc_set, 0, nullptr);
}

void engine_load_model(Engine* engine, uint32_t index, const char* filename) {
	if(index >= engine->model_count){
		std::cerr << "ERROR: Loading model at index " << index << " when there are only " << engine->model_count << " model elements\n";
	}
	
	load_model(engine->allocator, engine->models+index, filename);
}

void engine_create_pipeline_layout(Engine* engine) {
	// First we make our pipeline layout, specifically rasterization.
	// Our push constants are values we can give to our shader w/out
	// going through a buffer.
	VkPushConstantRange push_constant_range {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.size = sizeof(VkDeviceAddress)
	};
	// The only layout we need to define is that of our texture data.
	VkPipelineLayoutCreateInfo pipeline_layoutCI {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &engine->desc_set_layout_tex,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &push_constant_range
	};
	chk(vkCreatePipelineLayout(engine->device, &pipeline_layoutCI, nullptr, &engine->pipeline_layout), __LINE__);
}

void engine_load_shader(Engine* engine, uint32_t index, size_t data_size, const char* filename) {
	if(index >= engine->shader_count){
		std::cerr << "ERROR: Loading shader at index " << index << " when there are only " << engine->shader_count << " shader buffer elements\n";
	}

	// Allocate each shader data buffer and get their device
	// address so we can access them in shaders without descriptors
	engine->shader_data_buffers[index] = (ShaderDataBuffer*)malloc(sizeof(ShaderDataBuffer) * engine->frame_count);
	for(auto i = 0; i < engine->frame_count; i++) {
		VkBufferCreateInfo shader_data_bufferCI {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = data_size,
			.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		};
		VmaAllocationCreateInfo shader_data_buffer_allocCI {
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO
		};
		chk(vmaCreateBuffer(engine->allocator, &shader_data_bufferCI, &shader_data_buffer_allocCI, &engine->shader_data_buffers[index][i].buffer, &engine->shader_data_buffers[index][i].allocation, &engine->shader_data_buffers[index][i].allocation_info), __LINE__);
		VkBufferDeviceAddressInfo shader_data_buffer_device_address_info {
			.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
			.buffer = engine->shader_data_buffers[index][i].buffer
		};
		engine->shader_data_buffers[index][i].device_address = vkGetBufferDeviceAddress(engine->device, &shader_data_buffer_device_address_info);
	}

	// Loading Shaders
	// -------------------------------

	// Shaders need to be compiled to SPIR-V, we'll be writing them
	// in Slang for this engine so we need to convert them.
	// We can compile them offline via Slang's command line compiler,
	// but instead we'll just compile at runtime via Slang's library.

	// We start by making a "Slang Session"
	Slang::ComPtr<slang::IGlobalSession> slang_global_session;
	slang::createGlobalSession(slang_global_session.writeRef());
	auto slang_targets { std::to_array<slang::TargetDesc>({ {
		.format = SLANG_SPIRV,
		.profile = slang_global_session->findProfile("spriv_1_4")
	} })};
	auto slang_options { std::to_array<slang::CompilerOptionEntry>({ {
		slang::CompilerOptionName::EmitSpirvDirectly,
		{slang::CompilerOptionValueKind::Int, 1}
	} })};
	slang::SessionDesc slang_session_desc {
		.targets = slang_targets.data(),
		.targetCount = SlangInt(slang_targets.size()),
		.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
		.compilerOptionEntries = slang_options.data(),
		.compilerOptionEntryCount = uint32_t(slang_options.size())
	};
	Slang::ComPtr<slang::ISession> slang_session;
	slang_global_session->createSession(slang_session_desc, slang_session.writeRef());

	// Now we can load the shader file into SPIR-V format.
	Slang::ComPtr<slang::IModule> slang_module {
		slang_session->loadModuleFromSource("triangle", filename, nullptr, nullptr)
	};
	Slang::ComPtr<ISlangBlob> spirv_shader;
	slang_module->getTargetCode(0, spirv_shader.writeRef());

	// We need to make a module (container) for our shader to pass
	// into our graphics pipeline.
	VkShaderModuleCreateInfo shader_moduleCI {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = spirv_shader->getBufferSize(),
		.pCode = (uint32_t*)spirv_shader->getBufferPointer()
	};
	chk(vkCreateShaderModule(engine->device, &shader_moduleCI, nullptr, &engine->shader_modules[index]), __LINE__);
	// -------------------------------
}

void engine_create_basic_pipelines(Engine* engine, uint32_t* indices, uint32_t indices_size) {
	if(indices[indices_size-1] >= engine->shader_count){
		std::cerr << "ERROR: Creating pipeline at index " << indices[indices_size-1] << " when there are only " << engine->shader_count << " pipeline elements\n";
	}

	// OpenGL's pipeline is unoptimized because you can change state
	// any time, while Vulkan makes you "compile" (define) your
	// pipeline's state progression, allowing for optimization.
	// Still there are options to make some of your states dynamic.

	// Describe how our vertex attributes are layed out in memory.
	VkVertexInputBindingDescription vertex_binding_desc {
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	};
	VkVertexInputAttributeDescription vertex_attribute_descs[3] = {
		{ .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT },
		{ .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal) },
		{ .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, uv) }
	};

	// Now we define all of our state's create infos.
	VkPipelineVertexInputStateCreateInfo vertex_input_stateCI {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertex_binding_desc,
		.vertexAttributeDescriptionCount = 3,
		.pVertexAttributeDescriptions = vertex_attribute_descs
	};
	// If you want to do tesselation or other fun assembly stuff,
	// remember to change the topology to properly feed into them.
	VkPipelineInputAssemblyStateCreateInfo input_assembly_stateCI {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};

	// We will make our viewport dynamic since we don't want to
	// "recompile" the pipeline every time we resize the window.
	// The "scissor" is what defines the cutoff point where vertices
	// and fragments are discarded due to being offscreen.
	VkPipelineViewportStateCreateInfo viewport_stateCI {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1
	};
	VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamic_stateCI {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dynamic_states
	};
	VkPipelineDepthStencilStateCreateInfo depth_stencil_stateCI {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL // if depth is less (closer to camera) or equal, keep, else discard
	};
	// This object tells Vulkan we want to use dynamic rendering
	// instead of render passes (which are more annoying), it's newer
	// though, so we'll give it to our pipeline via pNext.
	VkPipelineRenderingCreateInfo renderingCI {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &engine->swapchainCI.imageFormat,
		.depthAttachmentFormat = engine->depth_format
	};
	// Set to default (we aren't really using them).
	VkPipelineColorBlendAttachmentState blend_attachment {
		.colorWriteMask = 0xF
	};
	VkPipelineColorBlendStateCreateInfo color_blend_stateCI {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &blend_attachment
	};
	// Set to default (we aren't really using them).
	VkPipelineRasterizationStateCreateInfo rasterization_stateCI {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.lineWidth = 1.0f
	};
	// Set to default (we aren't really using them).
	VkPipelineMultisampleStateCreateInfo multisample_stateCI {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};

	VkPipelineShaderStageCreateInfo* shader_stages = (VkPipelineShaderStageCreateInfo*)malloc(sizeof(VkPipelineShaderStageCreateInfo) * indices_size * 2);
	VkGraphicsPipelineCreateInfo* pipelineCIs = (VkGraphicsPipelineCreateInfo*)malloc(sizeof(VkGraphicsPipelineCreateInfo) * indices_size);
	for(auto i = 0; i < indices_size; i++) {
		shader_stages[i*2] = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = engine->shader_modules[indices[i]], .pName = "main" };
		shader_stages[i*2 + 1] = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = engine->shader_modules[indices[i]], .pName = "main" };

		// Now we set up the pipeline itself.
		VkGraphicsPipelineCreateInfo pipelineCI {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &renderingCI,
			.stageCount = 2,
			.pStages = &shader_stages[i*2],
			.pVertexInputState = &vertex_input_stateCI,
			.pInputAssemblyState = &input_assembly_stateCI,
			.pViewportState = &viewport_stateCI,
			.pRasterizationState = &rasterization_stateCI,
			.pMultisampleState = &multisample_stateCI,
			.pDepthStencilState = &depth_stencil_stateCI,
			.pColorBlendState = &color_blend_stateCI,
			.pDynamicState = &dynamic_stateCI,
			.layout = engine->pipeline_layout
		};
		if(i > 0) {
			pipelineCI.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
			pipelineCI.basePipelineIndex = 0;
		}
		else if(indices_size > 1) {
			pipelineCI.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
		}
		pipelineCIs[i] = pipelineCI;
	}
	VkPipeline* pipelines = (VkPipeline*)malloc(sizeof(VkPipeline) * indices_size);
	chk(vkCreateGraphicsPipelines(engine->device, VK_NULL_HANDLE, indices_size, pipelineCIs, nullptr, pipelines), __LINE__);
	free(shader_stages);
	free(pipelineCIs);
	for(auto i = 0; i < indices_size; i++) {
		engine->pipelines[indices[i]] = pipelines[i];
	}
	free(pipelines);
}

void engine_draw_model(Engine* engine, uint32_t m_index, uint32_t p_index, uint32_t s_index) {
	VkDeviceSize v_offset = 0;
	vkCmdBindPipeline(engine->command_buffers[engine->frame_index], VK_PIPELINE_BIND_POINT_GRAPHICS, engine->pipelines[p_index]);
	vkCmdBindDescriptorSets(engine->command_buffers[engine->frame_index], VK_PIPELINE_BIND_POINT_GRAPHICS, engine->pipeline_layout, 0, 1, &engine->desc_set, 0, nullptr);
	vkCmdBindVertexBuffers(engine->command_buffers[engine->frame_index], 0, 1, &engine->models[m_index].v_buffer, &v_offset);
	vkCmdBindIndexBuffer(engine->command_buffers[engine->frame_index], engine->models[m_index].v_buffer, engine->models[m_index].meshes[0].i_index, VK_INDEX_TYPE_UINT16);
	vkCmdPushConstants(engine->command_buffers[engine->frame_index], engine->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &engine->shader_data_buffers[s_index][engine->frame_index].device_address);

	vkCmdDrawIndexed(engine->command_buffers[engine->frame_index], engine->models[m_index].meshes[0].i_count, 3, 0, 0, 0);
}

void destroy_engine(Engine engine) {
	free(engine.shader_data_buffers);
	free(engine.command_buffers);
	chk(vkDeviceWaitIdle(engine.device), __LINE__);
	for(auto i = 0; i < engine.frame_count; i++) {
		vkDestroyFence(engine.device, engine.fences[i], nullptr);
		vkDestroySemaphore(engine.device, engine.image_acquired_semaphores[i], nullptr);
	}
	free(engine.fences);
	free(engine.image_acquired_semaphores);
	for(auto i = 0; i < engine.shader_count; i++) {
		vkDestroyShaderModule(engine.device, engine.shader_modules[i], nullptr);
		for(auto j = 0; j < engine.frame_count; j++) {
			vmaDestroyBuffer(engine.allocator, engine.shader_data_buffers[i][j].buffer, engine.shader_data_buffers[i][j].allocation);
		}
		free(engine.shader_data_buffers[i]);
	}
	free(engine.shader_data_buffers);
	free(engine.shader_modules);
	for(auto i = 0; i < engine.sc_image_count; i++) {
		vkDestroySemaphore(engine.device, engine.render_complete_semaphores[i], nullptr);
		vkDestroyImageView(engine.device, engine.sc_image_views[i], nullptr);
	}
	free(engine.render_complete_semaphores);
	vmaDestroyImage(engine.allocator, engine.depth_image, engine.depth_image_allocation);
	vkDestroyImageView(engine.device, engine.depth_image_view, nullptr);
	free(engine.sc_images);
	free(engine.sc_image_views);
	for(auto i = 0; i < engine.model_count; i++) {
		vmaDestroyBuffer(engine.allocator, engine.models[i].v_buffer, engine.models[i].v_buffer_allocation);
	}
	free(engine.models);
	for(auto i = 0; i < engine.texture_count; i++) {
		vkDestroyImageView(engine.device, engine.textures[i].view, nullptr);
		vkDestroySampler(engine.device, engine.textures[i].sampler, nullptr);
		vmaDestroyImage(engine.allocator, engine.textures[i].image, engine.textures[i].allocation);
	}
	free(engine.textures);
	free(engine.texture_descriptors);
	vkDestroyDescriptorSetLayout(engine.device, engine.desc_set_layout_tex, nullptr);
	vkDestroyDescriptorPool(engine.device, engine.desc_pool, nullptr);
	vkDestroyPipelineLayout(engine.device, engine.pipeline_layout, nullptr);
	for(auto i = 0; i < engine.shader_count; i++ ) {
		vkDestroyPipeline(engine.device, engine.pipelines[i], nullptr);
	}
	free(engine.pipelines);
	vkDestroySwapchainKHR(engine.device, engine.swapchain, nullptr);
	vkDestroySurfaceKHR(engine.instance, engine.surface, nullptr);
	vkDestroyCommandPool(engine.device, engine.command_pool, nullptr);

	vmaDestroyAllocator(engine.allocator);
	SDL_DestroyWindow(engine.window);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	SDL_Quit();
	vkDestroyDevice(engine.device, nullptr);
	vkDestroyInstance(engine.instance, nullptr);
}

