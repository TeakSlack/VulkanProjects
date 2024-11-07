#include <exception>
#include <iostream>
#include <vector>

#include "Globals.h"
#include "Vulkan.h"

void Run()
{
	Vulkan::logger->set_pattern("[%H:%M:%S %^%l%$]: %v"); // [Hour:Minute:Second Level]: Message
	Vulkan::CreateInstance();
	Vulkan::SetupDebugMessenger();

	Vulkan::DestroyInstance();
}

int main()
{
	try
	{
		Run();
	}
	catch (const std::exception& e)
	{
		Vulkan::logger->error(e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}