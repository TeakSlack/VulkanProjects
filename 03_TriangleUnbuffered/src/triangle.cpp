#include <exception>

#include <spdlog/spdlog.h>
#include <vulkan_app_base.h>

class TriangleBuffered : public VulkanAppBase
{
public:
	void run()
	{

	}
};

int main()
{
	try
	{
		TriangleBuffered app;

		app.run();
	}
	catch (std::exception& e)
	{
		spdlog::error(e.what());
	}
}