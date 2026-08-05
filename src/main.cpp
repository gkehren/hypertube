#include <iostream>
#include <curl/curl.h>
#include "App.hpp"
#include "ImGuiApp.hpp"

int main(int, char **)
{
	// Initialize cURL globally once on main thread before launching any workers
	curl_global_init(CURL_GLOBAL_DEFAULT);

	try
	{
		App app;
		app.initialize();
		ImGuiApp frontend(app);
		frontend.run();
		app.shutdown();
	}
	catch (const std::exception &e)
	{
		std::cerr << "An error occured: " << e.what() << std::endl;
		curl_global_cleanup();
		return EXIT_FAILURE;
	}

	curl_global_cleanup();
	return EXIT_SUCCESS;
}
