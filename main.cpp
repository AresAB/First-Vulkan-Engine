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

struct Vertex {
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv;
};
struct ShaderDataBuffer {
	VmaAllocation allocation{ VK_NULL_HANDLE };
	VmaAllocationInfo allocation_info{};
	VkBuffer buffer{ VK_NULL_HANDLE };
	VkDeviceAddress device_address{};
};
struct ShaderData {
	glm::mat4 projection;
	glm::mat4 view;
	glm::mat4 model[3];
	glm::vec4 light_pos{ 0.0f, -10.0f, 10.0f, 0.0f };
	uint32_t selected{1};
} shaderData{};
struct Texture {
	VmaAllocation allocation{ VK_NULL_HANDLE };
	VkImage image{ VK_NULL_HANDLE };
	VkImageView view{ VK_NULL_HANDLE };
	VkSampler sampler{ VK_NULL_HANDLE };
};

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

	// Use any of the available GPUs (w/ priority to discrete)
	// Throw error if no GPUs support Vulkan 1.3
	// Override if user asks for a specific GPU via command line
	uint32_t physical_device_index = num_physical_devices;

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

	if(argc > 1) {
		if(std::stoi(argv[1]) < num_physical_devices) {
			physical_device_index = std::stoi(argv[1]);
		}
		else { 
			std::cout << "ERROR: Input GPU index too high, defaulting back to automatic selection.\n";
		}
	}
	else if(physical_device_index == num_physical_devices) {
		std::cerr << "ERROR: No GPU found that supports Vulkan 1.3 or above\n";
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

	// Window Setup
	// - - - - - - - - - - - - - - - -
	SDL_Window* window = SDL_CreateWindow("First Window", 1280u, 720u, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

	// We interact with the window through its "surface" in Vulkan
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	chk(SDL_Vulkan_CreateSurface(window, vk_instance, NULL, &surface));

	VkSurfaceCapabilitiesKHR surface_caps;
	chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_caps));
	// - - - - - - - - - - - - - - - -

	// Swapchain Setup
	// -------------------------------

	// Default settings that will work on basically any machine
	// Real applications leave it up to user settings
	VkSwapchainCreateInfoKHR swapchainCI {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = surface_caps.minImageCount,
		.imageFormat = VK_FORMAT_B8G8R8A8_SRGB,
		.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
		.imageExtent{
			.width = surface_caps.currentExtent.width,
			.height = surface_caps.currentExtent.height
		},
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_FIFO_KHR
	};
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	chk(vkCreateSwapchainKHR(device, &swapchainCI, NULL, &swapchain));

	uint32_t sc_image_count = 0;
	chk(vkGetSwapchainImagesKHR(device, swapchain, &sc_image_count, NULL));
	VkImage* sc_images = (VkImage*)malloc(sc_image_count * sizeof(VkImage));
	chk(vkGetSwapchainImagesKHR(device, swapchain, &sc_image_count, sc_images));
	VkImageView* sc_image_views = (VkImageView*)malloc(sc_image_count * sizeof(VkImageView));
	// -------------------------------

	// Depth Testing
	// - - - - - - - - - - - - - - - -
	
	// If you want things to properly render in 3d, you'll need
	// to setup depth testing.
	// First, we get our depth buffer format.
	VkFormat depth_format_list[2] = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	VkFormat depth_format = VK_FORMAT_UNDEFINED;
	for(uint32_t i = 0; i < 2; i++) {
		VkFormatProperties2 format_props {
			.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2
		};
		vkGetPhysicalDeviceFormatProperties2(physical_device, depth_format_list[i], &format_props);
		if(format_props.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			depth_format = depth_format_list[i];
			break;
		}
	};

	// Then, we made our depth buffer (image).
	// Most of our options will be the default ones since
	// we effectively are just making a single empty image,
	// but we do need to tell it that it's a depth buffer (.usage).
	int window_width, window_height;
	SDL_GetWindowSize(window, &window_width, &window_height);
	VkImageCreateInfo depth_imageCI {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = depth_format,
		.extent {
			.width = (uint32_t)window_width,
			.height = (uint32_t)window_height,
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
	VkImage depth_image;
	VmaAllocation depth_image_allocation;
	// Allocate memory, can destroy it later via vmaDestroyImage()
	chk(vmaCreateImage(allocator, &depth_imageCI, &allocCI, &depth_image, &depth_image_allocation, NULL));

	// Now we need to get out image view, which is what allows us
	// to actually interact with the image.
	// The subresourceRange specifies what we want to access from
	// the image, such as the "aspect" (color, stencil, depth, etc),
	// mipmap level, and layer level.
	VkImageViewCreateInfo depth_viewCI {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = depth_image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = depth_format,
		.subresourceRange {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.levelCount = 1,
			.layerCount = 1
		}
	};
	VkImageView depth_image_view;
	chk(vkCreateImageView(device, &depth_viewCI, NULL, &depth_image_view));
	// - - - - - - - - - - - - - - - -
	
	// Loading Meshes
	// -------------------------------

	// attributes = vertex data, shapes = index data
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	chk(tinyobj::LoadObj(&attrib, &shapes, &materials, NULL, NULL, "assets/suzanne.obj"));

	// -Ligma
	// This creates a new vertex per index, which quite literally
	// removes the point of indices, so make sure to optimize later
	const VkDeviceSize index_count = shapes[0].mesh.indices.size();
	VkDeviceSize vertices_size = index_count * sizeof(Vertex);
	VkDeviceSize indices_size = index_count * sizeof(uint16_t);
	Vertex* vertices = (Vertex*)malloc(vertices_size);
	uint16_t* indices = (uint16_t*)malloc(indices_size);
	for(VkDeviceSize i = 0; i < index_count; i++) {
		auto index = shapes[0].mesh.indices[i];
		// Negate y values cause Vulkan is right-handed,
		// unlike OpenGL which is left-handed in coords
		Vertex v {
			.pos = {
				attrib.vertices[index.vertex_index * 3],
				-attrib.vertices[index.vertex_index * 3 + 1],
				attrib.vertices[index.vertex_index * 3 + 2]
			},
			.normal = {
				attrib.normals[index.normal_index * 3],
				-attrib.normals[index.normal_index * 3 + 1],
				attrib.normals[index.normal_index * 3 + 2]
			},
			.uv = {
				attrib.texcoords[index.texcoord_index * 2],
				1.0 - attrib.texcoords[index.texcoord_index * 2 + 1]
			}
		};
		vertices[i] = v;
		indices[i] = i;
	}

	// -Ligma
	// We put vertices and indices in the same buffer here,
	// might look into if this is truly ideal
	VkBufferCreateInfo bufferCI {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = vertices_size + indices_size,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
	};

	// First 2 flags tells VMA to create memory on the GPU (VRAM)
	// that is accessable to host. 3rd flags allows us to directly
	// copy data into VRAM.
	VmaAllocationCreateInfo v_buffer_allocCI {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};
	VmaAllocationInfo v_buffer_alloc_info;
	VkBuffer v_buffer;
	VmaAllocation v_buffer_allocation;
	chk(vmaCreateBuffer(allocator, &bufferCI, &v_buffer_allocCI, &v_buffer, &v_buffer_allocation, &v_buffer_alloc_info));
	memcpy(v_buffer_alloc_info.pMappedData, vertices, vertices_size);
	memcpy(((char*)v_buffer_alloc_info.pMappedData) + vertices_size, indices, indices_size);
	// -------------------------------
	
	// GPU and CPU Parallelism
	// - - - - - - - - - - - - - - - -

	// Create multiple frame buffers to allow the CPU to work on
	// frames while GPU is processing previous ones (refered to
	// commonly as "frames in flight").
	// Stick to only 2-3 frame buffers to avoid high latency.
	#define FRAME_COUNT 2
	ShaderDataBuffer shader_data_buffers[FRAME_COUNT];
	VkCommandBuffer command_buffers[FRAME_COUNT];
	VkFence fences[FRAME_COUNT];
	VkSemaphore image_acquired_semaphores[FRAME_COUNT];
	// - - - - - - - - - - - - - - - -
	
	// Shader Data Buffers
	// -------------------------------

	// Allocate each shader data buffer and get their device
	// address so we can access them in shaders without descriptors
	for(uint16_t i = 0; i < FRAME_COUNT; i++) {
		VkBufferCreateInfo shader_data_bufferCI {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizeof(ShaderData),
			.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		};
		VmaAllocationCreateInfo shader_data_buffer_allocCI {
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO
		};
		chk(vmaCreateBuffer(allocator, &shader_data_bufferCI, &shader_data_buffer_allocCI, &shader_data_buffers[i].buffer, &shader_data_buffers[i].allocation, &shader_data_buffers[i].allocation_info));
		VkBufferDeviceAddressInfo shader_data_buffer_device_address_info {
			.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
			.buffer = shader_data_buffers[i].buffer
		};
		shader_data_buffers[i].device_address = vkGetBufferDeviceAddress(device, &shader_data_buffer_device_address_info);
	}
	// -------------------------------

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
	for(uint16_t i = 0; i < FRAME_COUNT; i++) {
		chk(vkCreateFence(device, &fenceCI, nullptr, &fences[i]));
		chk(vkCreateSemaphore(device, &semaphoreCI, nullptr, &image_acquired_semaphores[i]));
	}
	VkSemaphore* render_complete_semaphores = (VkSemaphore*)malloc(sizeof(VkSemaphore) * sc_image_count);
	for(uint32_t i = 0; i < sc_image_count; i++) {
		chk(vkCreateSemaphore(device, &semaphoreCI, nullptr, render_complete_semaphores + i));
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
	VkCommandPool command_pool = VK_NULL_HANDLE;
	chk(vkCreateCommandPool(device, &command_poolCI, nullptr, &command_pool));

	VkCommandBufferAllocateInfo command_buffer_allocAI {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = command_pool,
		.commandBufferCount = FRAME_COUNT
	};
	chk(vkAllocateCommandBuffers(device, &command_buffer_allocAI, command_buffers));
	// -------------------------------

	// Loading Textures
	// - - - - - - - - - - - - - - - -

	// Textures are images, which aren't really as nice to work with
	// as buffers. We'll be using KTX, an image format created by
	// Khronos that naturally supports mipmaps, 3D textures, and
	// cubemaps. Here's a converter:
	// https://developer.imaginationtech.com/solutions/pvrtextool/
#define TEXTURE_COUNT 3
	Texture textures[TEXTURE_COUNT];
	VkDescriptorImageInfo texture_descriptors[TEXTURE_COUNT];
	uint32_t texture_count = static_cast<uint32_t>(TEXTURE_COUNT);
	for(uint32_t i = 0; i < texture_count; i++) {
		ktxTexture* ktx_texture = nullptr;
		std::string filename = "assets/suzanne" + std::to_string(i) + ".ktx";
		ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_texture);
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
		chk(vmaCreateImage(allocator, &tex_imgCI, &tex_image_allocCI, &textures[i].image, &textures[i].allocation, nullptr));
		VkImageViewCreateInfo tex_viewCI {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = textures[i].image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = tex_imgCI.format,
			.subresourceRange {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = ktx_texture->numLevels,
				.layerCount = 1
			}
		};
		chk(vkCreateImageView(device, &tex_viewCI, nullptr, &textures[i].view));

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
		chk(vmaCreateBuffer(allocator, &temp_image_bufferCI, &temp_image_allocCI, &temp_image_buffer, &temp_image_allocation, &temp_image_alloc_info));
		memcpy(temp_image_alloc_info.pMappedData, ktx_texture->pData, ktx_texture->dataSize);
		// Now we need to make our command buffers in order to
		// do the copy commands, and we'll want a fence to know
		// when it's done.
		VkFenceCreateInfo temp_fenceCI {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
		};
		VkFence temp_fence;
		chk(vkCreateFence(device, &temp_fenceCI, nullptr, &temp_fence));
		VkCommandBuffer temp_cb;
		VkCommandBufferAllocateInfo temp_cbAI {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = command_pool,
			.commandBufferCount = 1
		};
		chk(vkAllocateCommandBuffers(device, &temp_cbAI, &temp_cb));
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
		chk(vkBeginCommandBuffer(temp_cb, &temp_cbBI));
		VkImageMemoryBarrier2 barrier_tex_transfer {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.image = textures[i].image,
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
		vkCmdCopyBufferToImage(temp_cb, temp_image_buffer, textures[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(ktx_texture->numLevels), copy_regions);
		VkImageMemoryBarrier2 barrier_tex_read {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
			.image = textures[i].image,
			.subresourceRange {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = ktx_texture->numLevels,
				.layerCount = 1
			}
		};
		barrier_tex_info.pImageMemoryBarriers = &barrier_tex_read;
		vkCmdPipelineBarrier2(temp_cb, &barrier_tex_info);
		chk(vkEndCommandBuffer(temp_cb));
		VkSubmitInfo one_timeSI {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &temp_cb
		};
		chk(vkQueueSubmit(queue, 1, &one_timeSI, temp_fence));
		chk(vkWaitForFences(device, 1, &temp_fence, VK_TRUE, UINT64_MAX));
		free(copy_regions);
		vkDestroyFence(device, temp_fence, nullptr);
		vmaDestroyBuffer(allocator, temp_image_buffer, temp_image_allocation);
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
		chk(vkCreateSampler(device, &samplerCI, nullptr, &textures[i].sampler));
		ktxTexture_Destroy(ktx_texture);
		texture_descriptors[i] = {
			.sampler = textures[i].sampler,
			.imageView = textures[i].view,
			.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
		};
	}

	// Now that we have created our texture buffers, we need to
	// describe how they are formatted to the GPU via descriptors.
	// Unlike normal buffers, which have an extension we use to avoid
	// having to use descriptors, images do not have such luxury.
	// Descriptor indexing does help with the process luckily,
	// allowing us to put all our descriptors in an array and index
	// them in our shaders.
	
	// We are only using descriptors for images, so we make only one
	// binding (for images), and define our Set Layout, which defines
	// how our Application talks to our shaders.
	VkDescriptorBindingFlags desc_binding_flag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
	VkDescriptorSetLayoutBindingFlagsCreateInfo desc_set_binding_flagsCI {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.bindingCount = 1,
		.pBindingFlags = &desc_binding_flag
	};
	VkDescriptorSetLayoutBinding desc_set_layout_binding_tex{
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // we combine our texture images and samplers
		.descriptorCount = texture_count,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT // we are only accessing textures in fragment shaders for now
	};
	VkDescriptorSetLayoutCreateInfo desc_set_layout_texCI {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = &desc_set_binding_flagsCI,
		.bindingCount = 1,
		.pBindings = &desc_set_layout_binding_tex
	};
	VkDescriptorSetLayout desc_set_layout;
	chk(vkCreateDescriptorSetLayout(device, &desc_set_layout_texCI, nullptr, &desc_set_layout));

	VkDescriptorPoolSize desc_pool_size {
		.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = texture_count
	};
	VkDescriptorPoolCreateInfo desc_poolCI {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 1,
		.poolSizeCount = 1,
		.pPoolSizes = &desc_pool_size
	};
	VkDescriptorPool desc_pool;
	chk(vkCreateDescriptorPool(device, &desc_poolCI, nullptr, &desc_pool));

	// We are making 1 descriptor set, and enough descriptors for
	// each texture, as descriptor sets describe the interface, while
	// descriptors actually hold onto the data.
	VkDescriptorSetVariableDescriptorCountAllocateInfo var_desc_countAI {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
		.descriptorSetCount = 1,
		.pDescriptorCounts = &texture_count
	};
	VkDescriptorSetAllocateInfo desc_setAI {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = &var_desc_countAI,
		.descriptorPool = desc_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &desc_set_layout
	};
	VkDescriptorSet desc_set;
	chk(vkAllocateDescriptorSets(device, &desc_setAI, &desc_set));
	// We have allocated our descriptor set, now we need to fill it
	// with descriptors, which we set up in texture loading.
	VkWriteDescriptorSet write_desc_set {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = desc_set,
		.dstBinding = 0,
		.descriptorCount = texture_count,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = texture_descriptors
	};
	vkUpdateDescriptorSets(device, 1, &write_desc_set, 0, nullptr);
	// - - - - - - - - - - - - - - - -

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
		slang_session->loadModuleFromSource("triangle", "assets/shader.slang", nullptr, nullptr)
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
	VkShaderModule shader_module;
	chk(vkCreateShaderModule(device, &shader_moduleCI, nullptr, &shader_module));
	// -------------------------------

	// Graphics Pipeline
	// - - - - - - - - - - - - - - - -



	// - - - - - - - - - - - - - - - -



	chk(vkDeviceWaitIdle(device));
	for(uint16_t i = 0; i < FRAME_COUNT; i++) {
		vkDestroyFence(device, fences[i], nullptr);
		vkDestroySemaphore(device, image_acquired_semaphores[i], nullptr);
		vmaDestroyBuffer(allocator, shader_data_buffers[i].buffer, shader_data_buffers[i].allocation);
	}
	for(auto i = 0; i < sc_image_count; i++) {
		vkDestroySemaphore(device, render_complete_semaphores[i], nullptr);
		//vkDestroyImageView(device, sc_image_views[i], nullptr);
	}
	free(render_complete_semaphores);
	vmaDestroyImage(allocator, depth_image, depth_image_allocation);
	vkDestroyImageView(device, depth_image_view, nullptr);
	free(sc_images);
	free(sc_image_views);
	free(vertices);
	free(indices);
	vmaDestroyBuffer(allocator, v_buffer, v_buffer_allocation);
	for(uint16_t i = 0; i < texture_count; i++) {
		vkDestroyImageView(device, textures[i].view, nullptr);
		vkDestroySampler(device, textures[i].sampler, nullptr);
		vmaDestroyImage(allocator, textures[i].image, textures[i].allocation);
	}
	vkDestroyDescriptorSetLayout(device, desc_set_layout, nullptr);
	vkDestroyDescriptorPool(device, desc_pool, nullptr);
	vkDestroySwapchainKHR(device, swapchain, nullptr);
	vkDestroySurfaceKHR(vk_instance, surface, nullptr);
	vkDestroyCommandPool(device, command_pool, nullptr);

	vmaDestroyAllocator(allocator);
	SDL_DestroyWindow(window);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	SDL_Quit();
	vkDestroyDevice(device, nullptr);
	vkDestroyInstance(vk_instance, nullptr);
}
