#include <imgui/imconfig.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl3.h>
#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_internal.h>
#include <imgui/imgui.cpp>
#include <imgui/imgui_impl_sdl3.cpp>
#include <imgui/imgui_impl_vulkan.cpp>
#include <imgui/imgui_draw.cpp>
#include <imgui/imgui_tables.cpp>
#include <imgui/imgui_widgets.cpp>
#include <imgui/imgui_demo.cpp>

#include "engine.cpp"

VkDescriptorPool init_imgui(Engine* engine) {
	engine->imgui_enabled = true;

	VkDescriptorPoolSize pool_sizes[] = {
		{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
	};

	VkDescriptorPoolCreateInfo imgui_poolCI = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets = 1000,
		.poolSizeCount = 11,
		.pPoolSizes = pool_sizes
	};
	VkDescriptorPool imgui_pool;
	check_vk_result(vkCreateDescriptorPool(engine->device, &imgui_poolCI, nullptr, &imgui_pool));

	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplSDL3_InitForVulkan(engine->window);

	ImGui_ImplVulkan_InitInfo imgui_init_info = {
		.Instance = engine->instance,
		.PhysicalDevice = engine->physical_device,
		.Device = engine->device,
		.Queue = engine->queue,
		.DescriptorPool = imgui_pool,
		.MinImageCount = engine->frame_count,
		.ImageCount = engine->frame_count,
		.PipelineInfoMain = {
			.PipelineRenderingCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
				.colorAttachmentCount = 1,
				.pColorAttachmentFormats = &engine->swapchainCI.imageFormat,
				.depthAttachmentFormat = engine->depth_format
			}
		},
		.UseDynamicRendering = true
	};

	ImGui_ImplVulkan_Init(&imgui_init_info);

	return imgui_pool;
}

void destroy_imgui(Engine* engine, VkDescriptorPool imgui_pool) {
	chk(vkDeviceWaitIdle(engine->device), __LINE__);
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
	vkDestroyDescriptorPool(engine->device, imgui_pool, nullptr);
}
