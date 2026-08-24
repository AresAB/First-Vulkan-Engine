#include <engine.cpp>

struct ShaderData {
	glm::vec3 color{1.0f, 1.0f, 1.0f};
};

void engine_poll_events(Engine *engine) {
	SDL_Event event;
	while(SDL_PollEvent(&event)) {
		// Window Close Button
		if(event.type == SDL_EVENT_QUIT) {
			engine->closing = true;
			break;
		}
		// Window Resize Check
		if(event.type == SDL_EVENT_WINDOW_RESIZED) {
			engine->update_swapchain = true;
		}
	}
}

void engine_poll_scancodes(Engine* engine) {
	engine->deltatime = SDL_GetTicks() - engine->last_time / 1000.0f;
	engine->last_time = SDL_GetTicks();
	const bool* key_states = SDL_GetKeyboardState(NULL);

	// Quit Keybind
	if(key_states[SDL_SCANCODE_X] || key_states[SDL_SCANCODE_Q]) {
		engine->closing = true;
		return;
	}
}

void engine_begin_rendering(Engine* engine) {
	// Record command buffer
	chk(vkResetCommandBuffer(engine->command_buffers[engine->frame_index], 0), __LINE__);
	VkCommandBufferBeginInfo cbBI {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	chk(vkBeginCommandBuffer(engine->command_buffers[engine->frame_index], &cbBI), __LINE__);
	// Memory Barriers help transition layouts, and enforces
	// that it is done during the right pipeline stage.
	// srcStageMask is where the pipeline waits.
	// srcAccessMask defines what writes are to be available.
	// dst defines where and what is made visible.
	// Available: ready for memory operations.
	// Visible: able to be read
	VkImageMemoryBarrier2 output_barriers[2] {
		VkImageMemoryBarrier2 {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0,
			.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.image = engine->sc_images[engine->image_index],
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1
			}
		},
		VkImageMemoryBarrier2 {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
			.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
			.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.image = engine->depth_image,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
				.levelCount = 1,
				.layerCount = 1
			}
		}
	};
	VkDependencyInfo barrier_dependency_info {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 2,
		.pImageMemoryBarriers = output_barriers
	};
	// Inserts the memory transitions into command buffer.
	vkCmdPipelineBarrier2(engine->command_buffers[engine->frame_index], &barrier_dependency_info);

	VkRenderingAttachmentInfo color_attachment_info {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = engine->sc_image_views[engine->image_index],
		.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue{.color{0.0f,0.0f,0.2f,1.0f}}
	};
	VkRenderingAttachmentInfo depth_attachment_info {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = engine->depth_image_view,
		.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, // no need for it post-rendering
		.clearValue = {.depthStencil = {1.0f, 0}}
	};
	VkRenderingInfo rendering_info{
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea { .extent {
			.width = (uint32_t)engine->window_width,
			.height = (uint32_t)engine->window_height,
		}},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment_info,
		.pDepthAttachment = &depth_attachment_info
	};
	vkCmdBeginRendering(engine->command_buffers[engine->frame_index], &rendering_info);
	VkViewport viewport {
		.width = static_cast<float>(engine->window_width),
		.height = static_cast<float>(engine->window_height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};
	vkCmdSetViewport(engine->command_buffers[engine->frame_index], 0, 1, &viewport);
	VkRect2D scissor{ .extent{
		.width = (uint32_t)engine->window_width,
		.height = (uint32_t)engine->window_height,
	}};
	vkCmdSetScissor(engine->command_buffers[engine->frame_index], 0, 1, &scissor);
}

void engine_end_rendering_and_present(Engine* engine) {
	vkCmdEndRendering(engine->command_buffers[engine->frame_index]);
	VkImageMemoryBarrier2 barrier_present {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstAccessMask = 0,
		.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.image = engine->sc_images[engine->image_index],
		.subresourceRange{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.layerCount = 1
		}
	};
	VkDependencyInfo barrier_present_dependency_info {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrier_present
	};
	vkCmdPipelineBarrier2(engine->command_buffers[engine->frame_index], &barrier_present_dependency_info);
	chk(vkEndCommandBuffer(engine->command_buffers[engine->frame_index]), __LINE__);

	// Submit command buffer
	VkSemaphoreSubmitInfo wait_semaphore_info {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = engine->image_acquired_semaphores[engine->frame_index],
		.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	VkCommandBufferSubmitInfo command_buffer_submit_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.commandBuffer = engine->command_buffers[engine->frame_index]
	};
	VkSemaphoreSubmitInfo signal_semaphore_info {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = engine->render_complete_semaphores[engine->image_index],
		.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	VkSubmitInfo2 submit_info {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.waitSemaphoreInfoCount = 1,
		.pWaitSemaphoreInfos = &wait_semaphore_info,
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &command_buffer_submit_info,
		.signalSemaphoreInfoCount = 1,
		.pSignalSemaphoreInfos = &signal_semaphore_info
	};
	chk(vkQueueSubmit2(engine->queue, 1, &submit_info, engine->fences[engine->frame_index]), __LINE__);
	engine->frame_index = (engine->frame_index + 1) % engine->frame_count;
	// Present image
	VkPresentInfoKHR present_info {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &engine->render_complete_semaphores[engine->image_index],
		.swapchainCount = 1,
		.pSwapchains = &engine->swapchain,
		.pImageIndices = &engine->image_index
	};
	chk_swapchain(vkQueuePresentKHR(engine->queue, &present_info), &engine->update_swapchain, __LINE__);
}

void engine_render_loop(Engine engine) {
	engine.last_time = SDL_GetTicks();
	while(!engine.closing) {
		// Wait on fence, then acquire next image
		chk(vkWaitForFences(engine.device, 1, &engine.fences[engine.frame_index], true, UINT64_MAX), __LINE__);
		chk(vkResetFences(engine.device, 1, &engine.fences[engine.frame_index]), __LINE__);
		chk_swapchain(vkAcquireNextImageKHR(engine.device, engine.swapchain, UINT64_MAX, engine.image_acquired_semaphores[engine.frame_index], VK_NULL_HANDLE, &engine.image_index), &engine.update_swapchain, __LINE__);

		// First round of input handling
		engine_poll_events(&engine);
		// Recreate Swapchain
		if(engine.update_swapchain) {
			engine_recreate_swapchain(&engine);
		}

		engine_begin_rendering(&engine);

		// Update shader data and draw models
		// --------------------------
		engine_draw_model(&engine, 0, 0, 0, 1);
		// -------------------

		engine_end_rendering_and_present(&engine);

		// Poll events part 2
		engine_poll_scancodes(&engine);
	}
}

int main(int argc, char* argv[]) {
	std::string scene_filepath = argv[1];
	const char* tex = (argc > 2) ? argv[2] : "assets/end_times.ktx";
	ktxTexture* ktx_texture = nullptr;
	ktxTexture_CreateFromNamedFile(tex, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_texture);
	uint32_t w_w = ktx_texture->baseWidth;
	uint32_t w_h = ktx_texture->baseHeight;
	ktxTexture_Destroy(ktx_texture);
	while(w_w < 2880 || w_h < 1800) {
		w_w *= 2;
		w_h *= 2;
	}
	while(w_w > 2880 || w_h > 1800) {
		w_w /= 2;
		w_h /= 2;
	}

	EngineCreateInfo engineCI{ 
		.window_width = w_w,
		.window_height = w_h,
		.texture_count = 1,
		.model_count = 1,
		.shader_count = 1,
		.shader_data_buffer_count = 1,
		.is_silent = true
	};
	Engine engine = create_engine(engineCI);
	
	engine_load_texture_ktx(&engine, 0, tex);
	engine_load_texture_descriptors(&engine, VK_SHADER_STAGE_FRAGMENT_BIT);

	engine_load_model(&engine, 0, "assets/square.obj");

	engine_load_shader(&engine, 0, scene_filepath.c_str());
	uint32_t sdb_indices[1] = {0};
	engine_create_shader_data_buffers(&engine, sdb_indices, 1, sizeof(ShaderData));

	engine_create_pipeline_layout(&engine);
	uint32_t pipeline_indices[1] = {0};
	engine_create_basic_pipelines(&engine, pipeline_indices, 1);

	ShaderData data{};
	for(auto i = 0; i < engine.frame_count; i++) {
		memcpy(engine.shader_data_buffers[0][i].allocation_info.pMappedData, &data, sizeof(ShaderData));
	}
	engine_render_loop(engine);
	destroy_engine(engine);
}

