#include "vkEngine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vkTypes.h>
#include <vkInitializers.h>
#include <VkBootstrap.h>

#include <iostream>
#include <fstream>

#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			std::cout <<"Detected Vulkan error: " << err << std::endl; \
			abort();                                                \
		}                                                           \
	} while (0)


void vkEngine::VulkanEngine::init()
{
	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

	
	m_Window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		m_WindowExtent.width,
		m_WindowExtent.height,
		window_flags
	);
	
	init_vulkan();
	init_swapchain();
	init_commands();
	init_default_renderpass();
	init_framebuffers();
	init_sync_structures();
	init_pipeline();

	//everything went fine
	m_IsInitialized = true;
}
void vkEngine::VulkanEngine::cleanup()
{
	if (m_IsInitialized) 
	{

		//make sure the GPU has stopped doing its things
		vkWaitForFences(m_Device, 1, &m_RenderFence, true, 1000000000);

		m_MainDeletionQueue.flush();

		vkDestroyDevice(m_Device, nullptr);
		vkDestroySurfaceKHR(m_Instance, m_vkSurface, nullptr);
		vkb::destroy_debug_utils_messenger(m_Instance, m_DebugMessanger);
		vkDestroyInstance(m_Instance, nullptr);
		SDL_DestroyWindow(m_Window);
	}
}

void vkEngine::VulkanEngine::draw()
{
	VK_CHECK(vkWaitForFences(m_Device, 1, &m_RenderFence, true, 1000000000));
	VK_CHECK(vkResetFences	(m_Device, 1, &m_RenderFence));


	//request image from the swapchain, one second timeoutk
	uint32_t swapchainImageIndex ;
	VK_CHECK(vkAcquireNextImageKHR(m_Device, m_Swapchain, 1000000000, m_PresentSemaphore, nullptr, &swapchainImageIndex));
		

	//now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(m_MainCommandBuffer, 0));


	//naming it cmd for shorter writing
	VkCommandBuffer cmd = m_MainCommandBuffer;

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = {};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.pNext = nullptr;

	cmdBeginInfo.pInheritanceInfo = nullptr;
	cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));


	//make a clear-color from frame number. This will flash with a 120*pi frame period.
	VkClearValue clearValue;
	float flashBlue = abs(sin(m_FrameNumber / 120.f));
	float flashGreen = abs(cos(m_FrameNumber / 120.f));
	clearValue.color = { { 0.0f, flashGreen, flashBlue, 1.0f } };

	//start the main renderpass.
	//We will use the clear color from above, and the framebuffer of the index the swapchain gave us
	VkRenderPassBeginInfo rpInfo = {};
	rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpInfo.pNext = nullptr;

	rpInfo.renderPass = m_RenderPass;
	rpInfo.renderArea.offset.x = 0;
	rpInfo.renderArea.offset.y = 0;
	rpInfo.renderArea.extent = m_WindowExtent;
	rpInfo.framebuffer = m_Framebuffers[swapchainImageIndex];

	//connect clear values
	rpInfo.clearValueCount = 1;
	rpInfo.pClearValues = &clearValue;

	vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);


	if(m_ShaderIndex == 0)
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_TrianglePipeline);
	else 
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_SpecialTrianglePipeline);
	
	vkCmdDraw(cmd, 3, 1, 0, 0);

	//finalize the render pass
	vkCmdEndRenderPass(cmd);
	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));


		//prepare the submission to the queue.
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext = nullptr;

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	submit.pWaitDstStageMask = &waitStage;

	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &m_PresentSemaphore;

	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &m_RenderSemaphore;

	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;

	//submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &submit, m_RenderFence));


	// this will put the image we just rendered into the visible window.
	// we want to wait on the _renderSemaphore for that,
	// as it's necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;

	presentInfo.pSwapchains = &m_Swapchain;


	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &m_RenderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(m_GraphicsQueue, &presentInfo));

	//increase the number of frames drawn
	m_FrameNumber++;

	if (m_FrameNumber == 120)
		m_FrameNumber = 0;

}
void vkEngine::VulkanEngine::run()
{
	SDL_Event e;
	bool bQuit = false;

	//main loop
	while (!bQuit)
	{
		//Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{

			if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_SPACE) 
			{
				const uint32_t MaxPipelineNum = 2;
				m_ShaderIndex++;
				if (m_ShaderIndex == MaxPipelineNum) m_ShaderIndex = 0;
			}
				

			//close the window when user alt-f4s or clicks the X button			
			if (e.type == SDL_QUIT) bQuit = true;
		}
	

		draw();
	}
}
void vkEngine::VulkanEngine::init_commands()
{
	//create a command pool for commands submitted to the graphics queue.
	//we also want the pool to allow for resetting of individual command buffers
	VkCommandPoolCreateInfo commandPoolInfo = vkInit::command_pool_create_info(m_GraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &m_CommandPool));

	//allocate the default command buffer that we will use for rendering
	VkCommandBufferAllocateInfo cmdAllocInfo = vkInit::command_buffer_allocate_info(m_CommandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(m_Device, &cmdAllocInfo, &m_MainCommandBuffer));
	
	
	m_MainDeletionQueue.push_function([=]() {
		vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
	});

}
void vkEngine::VulkanEngine::init_swapchain()
{
	vkb::SwapchainBuilder swapchainBuilder{m_TargetGPU,m_Device,m_vkSurface};

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		.use_default_format_selection()
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(m_WindowExtent.width, m_WindowExtent.height)
		.build()
		.value();

	//store swapchain and its related images
	m_Swapchain = vkbSwapchain.swapchain;
	m_SwapchainImages = vkbSwapchain.get_images().value();
	m_SwapchainImageViews = vkbSwapchain.get_image_views().value();

	m_SwapchainImageFormat  = vkbSwapchain.image_format;


		m_MainDeletionQueue.push_function([=]() {
		vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
	});

}
void vkEngine::VulkanEngine::init_vulkan()
{
	vkb::InstanceBuilder builder;

	//make the Vulkan instance, with basic debug features
	auto inst_ret = builder.set_app_name("Vulkan App")
		.request_validation_layers(true)
		.require_api_version(1, 1, 0)
		.use_default_debug_messenger()
		.build();

	vkb::Instance vkb_inst = inst_ret.value();

	//store the instance
	m_Instance = vkb_inst.instance;
	//store the debug messenger
	m_DebugMessanger = vkb_inst.debug_messenger;

		// get the surface of the window we opened with SDL
	SDL_Vulkan_CreateSurface(m_Window, m_Instance, &m_vkSurface);

	//use vkbootstrap to select a GPU.
	//We want a GPU that can write to the SDL surface and supports Vulkan 1.1
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 1)
		.set_surface(m_vkSurface)
		.select()
		.value();

	//create the final Vulkan device
	vkb::DeviceBuilder deviceBuilder{ physicalDevice };

	vkb::Device vkbDevice = deviceBuilder.build().value();

	// Get the VkDevice handle used in the rest of a Vulkan application
	m_Device = vkbDevice.device;
	m_TargetGPU = physicalDevice.physical_device;


	// use vkbootstrap to get a Graphics queue
	m_GraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	m_GraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

}

void vkEngine::VulkanEngine::init_default_renderpass()
{


	// the renderpass will use this color attachment.
	VkAttachmentDescription color_attachment {};
	//the attachment will have the format needed by the swapchain
	color_attachment.format = m_SwapchainImageFormat;

	//1 sample, we won't be doing MSAA
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	// we Clear when this attachment is loaded
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// we keep the attachment stored when the renderpass ends
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	//we don't care about stencil
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	//we don't know or care about the starting layout of the attachment
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	//after the renderpass ends, the image has to be on a layout ready for display
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;






	VkAttachmentReference color_attachment_ref = {};
	//attachment number will index into the pAttachments array in the parent renderpass itself
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//we are going to create 1 subpass, which is the minimum you can do
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;




	//connect the color attachment to the info
	render_pass_info.attachmentCount = 1;
	render_pass_info.pAttachments = &color_attachment;
	//connect the subpass to the info
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;



	VK_CHECK(vkCreateRenderPass(m_Device, &render_pass_info, nullptr, &m_RenderPass));


	m_MainDeletionQueue.push_function([=]() {
		vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
    });
 

 } 
void vkEngine::VulkanEngine::init_framebuffers()
{	
	VkFramebufferCreateInfo fb_info = {};
	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.pNext = nullptr;

	fb_info.renderPass = m_RenderPass;
	fb_info.attachmentCount = 1;
	fb_info.width =  m_WindowExtent.width;
	fb_info.height = m_WindowExtent.height;
	fb_info.layers = 1;

	//grab how many images we have in the swapchain
	const uint32_t swapchain_imagecount = m_SwapchainImages.size();
	m_Framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

	//create framebuffers for each of the swapchain image views
	for (int i = 0; i < swapchain_imagecount; i++) {

		fb_info.pAttachments = &m_SwapchainImageViews[i];
		VK_CHECK(vkCreateFramebuffer(m_Device, &fb_info, nullptr, &m_Framebuffers[i]));

		m_MainDeletionQueue.push_function([=]() {
			vkDestroyFramebuffer(m_Device, m_Framebuffers[i], nullptr);
			vkDestroyImageView(m_Device, m_SwapchainImageViews[i], nullptr);
    	});
	}

}

void vkEngine::VulkanEngine::init_sync_structures()
{
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;

	//we want to create the fence with the Create Signaled flag, so we can wait on it before using it on a GPU command (for the first frame)
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VK_CHECK(vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &m_RenderFence));


	//enqueue the destruction of the fence
    m_MainDeletionQueue.push_function([=]() {
        vkDestroyFence(m_Device, m_RenderFence, nullptr);
    });

	//for the semaphores we don't need any flags
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = nullptr;
	semaphoreCreateInfo.flags = 0;

	VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_PresentSemaphore));
	VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_RenderSemaphore));


	//enqueue the destruction of semaphores
    m_MainDeletionQueue.push_function([=]() {
        vkDestroySemaphore(m_Device, m_PresentSemaphore, nullptr);
        vkDestroySemaphore(m_Device, m_RenderSemaphore, nullptr);
    });

}

void vkEngine::VulkanEngine::init_pipeline()
{
	VkShaderModule triangleVertexShader;
	if (!load_shader_module("../../shaders/triangleShader.vert.spv", &triangleVertexShader))
	{
		std::cout << "Error when building the triangle vertex shader module" << std::endl;

	}
	else {
		std::cout << "Triangle vertex shader successfully loaded" << std::endl;
	}


	VkShaderModule triangleFragShader;
	if (!load_shader_module("../../shaders/triangleShader.frag.spv", &triangleFragShader))
	{
		std::cout << "Error when building the triangle fragment shader module" << std::endl;
	}
	else {
		std::cout << "Triangle fragment shader successfully loaded" << std::endl;
	}
	

	VkShaderModule specialTriangleVertexShader;
	if (!load_shader_module("../../shaders/specialTriangleShader.vert.spv", &specialTriangleVertexShader))
	{
	std::cout << "Error when building the triangle vertex shader module" << std::endl;
	
	}
	else {
	std::cout << "Triangle vertex shader successfully loaded" << std::endl;
	}
	
	
	VkShaderModule specialTriangleFragShader;
	if (!load_shader_module("../../shaders/specialTriangleShader.frag.spv", &specialTriangleFragShader))
	{
	std::cout << "Error when building the triangle fragment shader module" << std::endl;
	}
	else {
	std::cout << "Triangle fragment shader successfully loaded" << std::endl;
	}
	
	//build the pipeline layout that controls the inputs/outputs of the shader
	//we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
	VkPipelineLayoutCreateInfo pipeline_layout_info = vkInit::pipeline_layout_create_info();

	VK_CHECK(vkCreatePipelineLayout(m_Device, &pipeline_layout_info, nullptr, &m_TrianglePipelineLayout));


		//build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
	PipelineBuilder pipelineBuilder;

	pipelineBuilder.m_ShaderStages.push_back(
		vkInit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, triangleVertexShader));

	pipelineBuilder.m_ShaderStages.push_back(
		vkInit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFragShader));


	//vertex input controls how to read vertices from vertex buffers. We aren't using it yet
	pipelineBuilder.m_VertexInputInfo = vkInit::vertex_input_state_create_info();

	//input assembly is the configuration for drawing triangle lists, strips, or individual points.
	//we are just going to draw triangle list
	pipelineBuilder.m_InputAssembly = vkInit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	//build viewport and scissor from the swapchain extents
	pipelineBuilder.m_Viewport.x = 0.0f;
	pipelineBuilder.m_Viewport.y = 0.0f;
	pipelineBuilder.m_Viewport.width = (float) m_WindowExtent.width;
	pipelineBuilder.m_Viewport.height = (float)m_WindowExtent.height;
	pipelineBuilder.m_Viewport.minDepth = 0.0f;
	pipelineBuilder.m_Viewport.maxDepth = 1.0f;

	pipelineBuilder.m_Scissor.offset = { 0, 0 };
	pipelineBuilder.m_Scissor.extent = m_WindowExtent;

	//configure the rasterizer to draw filled triangles
	pipelineBuilder.m_Rasterizer = vkInit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

	//we don't use multisampling, so just run the default one
	pipelineBuilder.m_Multisampling = vkInit::multisampling_state_create_info();

	//a single blend attachment with no blending and writing to RGBA
	pipelineBuilder.m_ColorBlendAttachment = vkInit::color_blend_attachment_state();

	//use the triangle layout we created
	pipelineBuilder.m_PipelineLayout = m_TrianglePipelineLayout;
	

	//finally build the pipeline
	m_TrianglePipeline = pipelineBuilder.build_pipeline(m_Device, m_RenderPass);

	//clear the shader stages for the builder
	pipelineBuilder.m_ShaderStages.clear();

	//add the other shaders
	pipelineBuilder.m_ShaderStages.push_back(
		vkInit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, specialTriangleVertexShader));


	pipelineBuilder.m_ShaderStages.push_back(
		vkInit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, specialTriangleFragShader));

	m_SpecialTrianglePipeline = pipelineBuilder.build_pipeline(m_Device, m_RenderPass);



	//destroy all shader modules, outside of the queue
	vkDestroyShaderModule(m_Device, specialTriangleVertexShader, nullptr);
	vkDestroyShaderModule(m_Device, specialTriangleFragShader, nullptr);
	vkDestroyShaderModule(m_Device, triangleFragShader, nullptr);
	vkDestroyShaderModule(m_Device, triangleVertexShader, nullptr);

 	m_MainDeletionQueue.push_function([=]() {
		//destroy the 2 pipelines we have created
		vkDestroyPipeline(m_Device, m_SpecialTrianglePipeline, nullptr);
        vkDestroyPipeline(m_Device, m_TrianglePipeline, nullptr);

		//destroy the pipeline layout that they use
		vkDestroyPipelineLayout(m_Device, m_TrianglePipelineLayout, nullptr);
    });

}

bool vkEngine::VulkanEngine::load_shader_module(const char* filePath, VkShaderModule* outShaderModule)
{
	//open the file. With cursor at the end
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open()) 
	{
		return false;
	}
	//find what the size of the file is by looking up the location of the cursor
	//because the cursor is at the end, it gives the size directly in bytes
	size_t fileSize = (size_t)file.tellg();

	//spirv expects the buffer to be on uint32, so make sure to reserve an int vector big enough for the entire file
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	//put file cursor at beginning
	file.seekg(0);

	//load the entire file into the buffer
	file.read((char*)buffer.data(), fileSize);

	//now that the file is loaded into the buffer, we can close it
	file.close();



	//create a new shader module, using the buffer we loaded
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.pNext = nullptr;

	//codeSize has to be in bytes, so multiply the ints in the buffer by size of int to know the real size of the buffer
	createInfo.codeSize = buffer.size() * sizeof(uint32_t);
	createInfo.pCode = buffer.data();

	//check that the creation goes well.
	VkShaderModule shaderModule;
	if (vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		return false;
	}
	*outShaderModule = shaderModule;
	return true;
}

VkPipeline vkEngine::PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass pass)
{

			//make viewport state from our stored viewport and scissor.
			//at the moment we won't support multiple viewports or scissors

			VkPipelineViewportStateCreateInfo viewportState = {};
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.pNext = nullptr;

			viewportState.viewportCount = 1;
			viewportState.pViewports = &m_Viewport;
			viewportState.scissorCount = 1;
			viewportState.pScissors = &m_Scissor;

			//setup dummy color blending. We aren't using transparent objects yet
			//the blending is just "no blend", but we do write to the color attachment
			VkPipelineColorBlendStateCreateInfo colorBlending = {};
			colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlending.pNext = nullptr;

			colorBlending.logicOpEnable = VK_FALSE;
			colorBlending.logicOp = VK_LOGIC_OP_COPY;
			colorBlending.attachmentCount = 1;
			colorBlending.pAttachments = &m_ColorBlendAttachment;



			//build the actual pipeline
			//we now use all of the info structs we have been writing into into this one to create the pipeline
			VkGraphicsPipelineCreateInfo pipelineInfo = {};
			pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineInfo.pNext = nullptr;

			pipelineInfo.stageCount = m_ShaderStages.size();
			pipelineInfo.pStages = m_ShaderStages.data();
			pipelineInfo.pVertexInputState = &m_VertexInputInfo;
			pipelineInfo.pInputAssemblyState = &m_InputAssembly;
			pipelineInfo.pViewportState = &viewportState;
			pipelineInfo.pRasterizationState = &m_Rasterizer;
			pipelineInfo.pMultisampleState = &m_Multisampling;
			pipelineInfo.pColorBlendState = &colorBlending;
			pipelineInfo.layout = m_PipelineLayout;
			pipelineInfo.renderPass = pass;
			pipelineInfo.subpass = 0;
			pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

			//it's easy to error out on create graphics pipeline, so we handle it a bit better than the common VK_CHECK case
			VkPipeline newPipeline;
			if (vkCreateGraphicsPipelines(
				device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
				std::cout << "failed to create pipeline\n";
				return VK_NULL_HANDLE; // failed to create graphics pipeline
			}
			else
			{
				return newPipeline;
			}


}
