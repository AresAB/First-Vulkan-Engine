#include <engine.cpp>

struct ShaderData {
	glm::mat4 projection;
	glm::mat4 view;
	glm::mat4 model;
	glm::vec4 light_pos{ 0.0f, -10.0f, 10.0f, 0.0f };
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
		// Wireframe Mode Keybind
		if(engine->wireframe_enabled && event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_E) {
			engine->is_wireframe = !engine->is_wireframe;
		}
		// Screenshot Keybind
		if(event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_R) {
			VkMemoryRequirements image_mem_reqs;
			vkGetImageMemoryRequirements(engine->device, engine->sc_images[engine->image_index], &image_mem_reqs);

			VkBuffer trans_image_buffer;
			VmaAllocation trans_image_allocation;
			VkBufferCreateInfo trans_image_bufferCI {
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = image_mem_reqs.size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
			};
			VmaAllocationCreateInfo trans_image_allocCI {
				.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
				.usage = VMA_MEMORY_USAGE_AUTO
			};
			VmaAllocationInfo trans_image_alloc_info;
			chk(vmaCreateBuffer(engine->allocator, &trans_image_bufferCI, &trans_image_allocCI, &trans_image_buffer, &trans_image_allocation, &trans_image_alloc_info), __LINE__);

			VkCommandBuffer scrn_shot_cb;
			VkCommandBufferAllocateInfo scrn_shot_cbAI {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = engine->command_pool,
				.commandBufferCount = 1
			};
			chk(vkAllocateCommandBuffers(engine->device, &scrn_shot_cbAI, &scrn_shot_cb), __LINE__);
			VkCommandBufferBeginInfo scrn_shot_cbBI {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
			};
			chk(vkBeginCommandBuffer(scrn_shot_cb, &scrn_shot_cbBI), __LINE__);
			VkImageMemoryBarrier2 mb_transfer {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask = 0,
				.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
				.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				.image = engine->sc_images[engine->image_index],
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = 1,
					.layerCount = 1
				}
			};
			VkDependencyInfo transfer_info {
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers = &mb_transfer
			};
			vkCmdPipelineBarrier2(scrn_shot_cb, &transfer_info);
			VkBufferImageCopy copy_region {
				.bufferOffset = 0,
				.imageSubresource {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = 0,
					.layerCount = 1
				},
				.imageExtent {
					.width = engine->surface_caps.currentExtent.width,
					.height = engine->surface_caps.currentExtent.height,
					.depth = 1
				}
			};
			vkCmdCopyImageToBuffer(scrn_shot_cb, engine->sc_images[engine->image_index], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, trans_image_buffer, 1, &copy_region);
			chk(vkEndCommandBuffer(scrn_shot_cb), __LINE__);
			VkSemaphoreSubmitInfo scrn_shot_wait_semaphore_info {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = engine->image_acquired_semaphores[engine->frame_index],
				.stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT
			};
			VkCommandBufferSubmitInfo scrn_shot_command_buffer_submit_info {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
				.commandBuffer = scrn_shot_cb
			};
			VkSemaphoreSubmitInfo scrn_shot_signal_semaphore_info {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = engine->image_acquired_semaphores[engine->frame_index],
				.stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT
			};
			VkSubmitInfo2 scrn_shot_submit_info {
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
				.waitSemaphoreInfoCount = 1,
				.pWaitSemaphoreInfos = &scrn_shot_wait_semaphore_info,
				.commandBufferInfoCount = 1,
				.pCommandBufferInfos = &scrn_shot_command_buffer_submit_info,
				.signalSemaphoreInfoCount = 1,
				.pSignalSemaphoreInfos = &scrn_shot_signal_semaphore_info
			};
			chk(vkQueueSubmit2(engine->queue, 1, &scrn_shot_submit_info, engine->fences[engine->frame_index]), __LINE__);
			chk(vkWaitForFences(engine->device, 1, &engine->fences[engine->frame_index], VK_TRUE, UINT64_MAX), __LINE__);
			chk(vkResetFences(engine->device, 1, &engine->fences[engine->frame_index]), __LINE__);

			ktxTextureCreateInfo scrn_shotCI{
				.vkFormat = engine->swapchainCI.imageFormat,
				.baseWidth = (ktx_uint32_t)engine->surface_caps.currentExtent.width,
				.baseHeight = (ktx_uint32_t)engine->surface_caps.currentExtent.height,
				.baseDepth = 1,
				.numDimensions = 2,
				.numLevels = 1,
				.numLayers = 1,
				.numFaces = 1,
				.isArray = KTX_FALSE,
				.generateMipmaps = KTX_FALSE
			};
			ktxTexture2* scrn_shot;
			bool no_error = true;
			KTX_error_code err;
			err = ktxTexture2_Create(&scrn_shotCI, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &scrn_shot);
			if(err != KTX_SUCCESS) {
				std::cerr << "KTX ERROR: Failed to initialize ktxTexture for screenshot at line " << __LINE__ << "\nKTX ERROR " << ktxErrorString(err) << "\n";
				no_error = false;
			}
			memcpy(scrn_shot->pData, trans_image_alloc_info.pMappedData, scrn_shot->dataSize);
			if(err != KTX_SUCCESS) {
				std::cerr << "KTX ERROR: Failed to copy image buffer data into ktxTexture for screenshot at line " << __LINE__ << "\nKTX ERROR " << ktxErrorString(err) << "\n";
				no_error = false;
			}
			vmaDestroyBuffer(engine->allocator, trans_image_buffer, trans_image_allocation);
			uint32_t scrn_shot_i = 0;
			std::filesystem::path scrn_shot_path = "./results/untitled_" + std::to_string(scrn_shot_i) + ".ktx";
			if(no_error) {
			while(std::filesystem::exists(scrn_shot_path.string())) {
				scrn_shot_i++;
				scrn_shot_path = "./results/untitled_" + std::to_string(scrn_shot_i) + ".ktx";
			}
			err = ktxTexture2_WriteToNamedFile(scrn_shot, scrn_shot_path.string().c_str());
			if(err != KTX_SUCCESS) {
				std::cerr << "KTX ERROR: Failed to write screen shot ktxTexture to file at line " << __LINE__ << "\nKTX ERROR " << ktxErrorString(err) << "\n";
			}
			else {
				std::cout << "Screenshot \"" << scrn_shot_path.string() << "\" taken\n";
			}}
			ktxTexture2_Destroy(scrn_shot);
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

	// Camera Controls
	float cam_mv_spd = engine->cam_mv_spd;
	float cam_rot_spd = engine->cam_rot_spd;
	float cam_plane_spd = engine->cam_plane_spd;
	float cam_zoom_spd = engine->cam_zoom_spd;
	if(key_states[SDL_SCANCODE_LSHIFT] || key_states[SDL_SCANCODE_RSHIFT]) {
		cam_mv_spd *= 2;
		cam_rot_spd *= 2;
		cam_plane_spd *= 2;
		cam_zoom_spd *= 2;
	}
	if(key_states[SDL_SCANCODE_LCTRL] || key_states[SDL_SCANCODE_RCTRL]) {
		cam_mv_spd *= 0.25;
		cam_rot_spd *= 0.25;
		cam_plane_spd *= 0.25;
	}
	if(key_states[SDL_SCANCODE_A]) {
		engine->cam_pos += cam_mv_spd * engine->deltatime * glm::vec3(1.0f, 0.0f, 0.0f) * glm::mat3(engine->cam_rot_mat);
	}
	if(key_states[SDL_SCANCODE_D]) {
		engine->cam_pos -= cam_mv_spd * engine->deltatime * glm::vec3(1.0f, 0.0f, 0.0f) * glm::mat3(engine->cam_rot_mat);
	}
	if(key_states[SDL_SCANCODE_W]) {
		engine->cam_pos += cam_mv_spd * engine->deltatime * glm::vec3(0.0f, 1.0f, 0.0f) * glm::mat3(engine->cam_rot_mat);
	}
	if(key_states[SDL_SCANCODE_S]) {
		engine->cam_pos -= cam_mv_spd * engine->deltatime * glm::vec3(0.0f, 1.0f, 0.0f) * glm::mat3(engine->cam_rot_mat);
	}
	if(key_states[SDL_SCANCODE_K]) {
		engine->cam_pos += cam_mv_spd * engine->deltatime * glm::vec3(0.0f, 0.0f, 1.0f) * glm::mat3(engine->cam_rot_mat);
	}
	if(key_states[SDL_SCANCODE_J]) {
		engine->cam_pos -= cam_mv_spd * engine->deltatime * glm::vec3(0.0f, 0.0f, 1.0f) * glm::mat3(engine->cam_rot_mat);
	}
	if(key_states[SDL_SCANCODE_H]) {
		engine->cam_rot_mat = glm::rotate(engine->cam_rot_mat, 6.28f * cam_rot_spd, glm::vec3(0.0f, 0.0f, 1.0f) * glm::mat3(engine->cam_rot_mat));
	}
	if(key_states[SDL_SCANCODE_L]) {
		engine->cam_rot_mat = glm::rotate(engine->cam_rot_mat, 6.28f * cam_rot_spd, glm::vec3(0.0f, 0.0f, -1.0f) * glm::mat3(engine->cam_rot_mat));
	}
	if(key_states[SDL_SCANCODE_UP]) {
		engine->cam_rot_mat = glm::rotate(engine->cam_rot_mat, 6.28f * cam_rot_spd, glm::vec3(1.0f, 0.0f, 0.0f) * glm::mat3(engine->cam_rot_mat));
	}
	if(key_states[SDL_SCANCODE_DOWN]) {
		engine->cam_rot_mat = glm::rotate(engine->cam_rot_mat, 6.28f * cam_rot_spd, glm::vec3(-1.0f, 0.0f, 0.0f) * glm::mat3(engine->cam_rot_mat));
	}
	if(key_states[SDL_SCANCODE_LEFT]) {
		engine->cam_rot_mat = glm::rotate(engine->cam_rot_mat, 6.28f * cam_rot_spd, glm::vec3(0.0f, -1.0f, 0.0f) * glm::mat3(engine->cam_rot_mat));
	}
	if(key_states[SDL_SCANCODE_RIGHT]) {
		engine->cam_rot_mat = glm::rotate(engine->cam_rot_mat, 6.28f * cam_rot_spd, glm::vec3(0.0f, 1.0f, 0.0f) * glm::mat3(engine->cam_rot_mat));
	}
	if(key_states[SDL_SCANCODE_U]) {
		engine->cam_pos = engine->og_cam_pos;
	}
	if(key_states[SDL_SCANCODE_I]) {
		engine->cam_rot_mat = engine->og_cam_rot_mat;
	}
	if(key_states[SDL_SCANCODE_F]) {
		engine->cam_f_plane += cam_plane_spd;
	}
	if(key_states[SDL_SCANCODE_G]) {
		engine->cam_n_plane += cam_plane_spd;
	}
	if(key_states[SDL_SCANCODE_C]) {
		engine->cam_f_plane -= cam_plane_spd;
	}
	if(key_states[SDL_SCANCODE_V]) {
		engine->cam_n_plane -= cam_plane_spd;
	}
	if(key_states[SDL_SCANCODE_B]) {
		engine->cam_f_plane = engine->og_cam_f_plane;
	}
	if(key_states[SDL_SCANCODE_N]) {
		engine->cam_n_plane = engine->og_cam_n_plane;
	}
	if(key_states[SDL_SCANCODE_T]) {
		engine->cam_zoom += cam_zoom_spd;
	}
	if(key_states[SDL_SCANCODE_Y]) {
		engine->cam_zoom -= cam_zoom_spd;
	}
	if(key_states[SDL_SCANCODE_O]) {
		engine->cam_zoom = engine->og_cam_zoom;
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
		ShaderData data{};
		data.projection = glm::perspective(glm::radians(engine.cam_zoom), (float)engine.window_width / (float)engine.window_height, engine.cam_n_plane, engine.cam_f_plane);
		data.view = glm::translate(engine.cam_rot_mat, engine.cam_pos);
		glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f));
		data.model = glm::scale(model, glm::vec3(1.0f));
		memcpy(engine.shader_data_buffers[0][engine.frame_index].allocation_info.pMappedData, &data, sizeof(ShaderData));
		engine_draw_model(&engine, 0, 0, 0, 1);

		model = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 0.0f, 0.0f));
		data.model = glm::scale(model, glm::vec3(1.0f));
		memcpy(engine.shader_data_buffers[1][engine.frame_index].allocation_info.pMappedData, &data, sizeof(ShaderData));
		engine_draw_model(&engine, 0, 0, 1, 1);
		// -------------------

		engine_end_rendering_and_present(&engine);

		// Poll events part 2
		engine_poll_scancodes(&engine);
	}
}

int main(int argc, char* argv[]) {
	std::string scene_filepath = argv[1];

	EngineCreateInfo engineCI{ 
		.texture_count = 1,
		.model_count = 1,
		.shader_count = 1,
		.shader_data_buffer_count = 2,
		.wireframe_enabled = true
	};
	Engine engine = create_engine(engineCI);
	
	engine_load_texture_ktx(&engine, 0, "assets/suzanne0.ktx");
	engine_load_texture_descriptors(&engine, VK_SHADER_STAGE_FRAGMENT_BIT);

	engine_load_model(&engine, 0, "assets/suzanne.obj");

	engine_load_shader(&engine, 0, sizeof(ShaderData), (scene_filepath + "/shaders/shader.slang").c_str());
	uint32_t sdb_indices[2] = {0,1};
	engine_create_shader_data_buffers(&engine, sdb_indices, 2, sizeof(ShaderData));

	engine_create_pipeline_layout(&engine);
	uint32_t pipeline_indices[1] = {0};
	engine_create_basic_pipelines(&engine, pipeline_indices, 1);

	engine_render_loop(engine);
	destroy_engine(engine);
}

