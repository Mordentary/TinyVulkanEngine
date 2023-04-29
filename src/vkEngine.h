// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vkTypes.h>
#include <vector>
#include <deque>
#include <functional>
struct SDL_Window;

namespace vkEngine {

	struct DeletionQueue
	{
	public:
		void push_function(std::function<void()>&& function) 
		{
			deletors.push_back(function);
		}

		void flush() 
		{
			// reverse iterate the deletion queue to execute all the functions
			for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
				(*it)(); //call the function
			}

			deletors.clear();
		}
	private: 
		std::deque<std::function<void()>> deletors;
	};

	class VulkanEngine {
	public:
		//initializes everything in the engine
		void init();
		//shuts down the engine
		void cleanup();
		//draw loop
		void draw();
		//run main loop
		void run();

	private:
		void init_commands();

		void init_swapchain();

		void init_vulkan();

		void init_default_renderpass();

		void init_framebuffers();
	
		void init_sync_structures();

		void init_pipeline();

		//loads a shader module from a spir-v file. Returns false if it errors
		bool load_shader_module(const char* filePath, VkShaderModule* outShaderModule);


	private:
		VkExtent2D m_WindowExtent{ 1240 , 720 };
		SDL_Window* m_Window{ nullptr };
		bool m_IsInitialized{ false };
		int m_FrameNumber{ 0 };
		uint32_t m_ShaderIndex{0};


		VkInstance m_Instance;
		VkPhysicalDevice m_TargetGPU;
		VkDevice m_Device;
		VkSurfaceKHR m_vkSurface;

		VkQueue m_GraphicsQueue;
		uint32_t m_GraphicsQueueFamily;

		VkCommandPool m_CommandPool;
		VkCommandBuffer m_MainCommandBuffer;

		VkRenderPass m_RenderPass;
		std::vector<VkFramebuffer> m_Framebuffers;

		
		VkPipelineLayout m_TrianglePipelineLayout;

		VkPipeline m_TrianglePipeline;
		VkPipeline m_SpecialTrianglePipeline;
		VkSemaphore m_PresentSemaphore, m_RenderSemaphore;
		VkFence m_RenderFence;

		VkDebugUtilsMessengerEXT m_DebugMessanger;

		DeletionQueue m_MainDeletionQueue;
	private:
		VkSwapchainKHR m_Swapchain; 
		//image format expected by the windowing system
		VkFormat m_SwapchainImageFormat;
		//array of images from the swapchain
		std::vector<VkImage> m_SwapchainImages;
		//array of image-views from the swapchain
		std::vector<VkImageView> m_SwapchainImageViews;

	};


	class PipelineBuilder 
	{



	public: 
		std::vector<VkPipelineShaderStageCreateInfo> m_ShaderStages;
		VkPipelineVertexInputStateCreateInfo m_VertexInputInfo;
		VkPipelineInputAssemblyStateCreateInfo m_InputAssembly;
		VkViewport m_Viewport;
		VkRect2D m_Scissor;
		VkPipelineRasterizationStateCreateInfo m_Rasterizer;
		VkPipelineColorBlendAttachmentState m_ColorBlendAttachment;
		VkPipelineMultisampleStateCreateInfo m_Multisampling;
		VkPipelineLayout m_PipelineLayout;
	public:
		VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);




	};


}
