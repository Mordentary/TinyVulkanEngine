#include <vkEngine.h>

int main(int argc, char* argv[])
{
	vkEngine::VulkanEngine engine;

	engine.init();	
	
	engine.run();	

	engine.cleanup();	

	return 0;
}
