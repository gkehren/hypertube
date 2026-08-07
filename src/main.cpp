#include "SlintAppController.hpp"

#include <curl/curl.h>
#include <iostream>

#include "App.hpp"

int main()
{
	if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
	{
		std::cerr << "Failed to initialize cURL" << std::endl;
		return 1;
	}

	int exitCode = 0;
	try
	{
		App app;
		Result initResult = app.initialize();
		if (!initResult)
		{
			std::cerr << "Fatal initialization error: " << initResult.message << std::endl;
			curl_global_cleanup();
			return 1;
		}
		int controllerExitCode = 0;
		{
			auto window = MainWindow::create();
			SlintAppController controller(app, window);
			controller.bind();
			controller.start();
			window->run();

			const Result stopResult = controller.stop();
			if (!stopResult)
			{
				std::cerr << "Failed to finalize Slint preferences: " << stopResult.message << std::endl;
				controllerExitCode = 1;
			}
		}
		app.shutdown();
		exitCode = controllerExitCode;
	}
	catch (const std::exception &error)
	{
		std::cerr << "An error occurred: " << error.what() << std::endl;
		exitCode = 1;
	}

	curl_global_cleanup();
	return exitCode;
}
