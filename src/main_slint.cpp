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
		app.initialize();
		auto window = MainWindow::create();
		SlintAppController controller(app, window);
		controller.bind();
		controller.start();
		slint::run_event_loop();
		controller.stop();
		app.shutdown();
	}
	catch (const std::exception &error)
	{
		std::cerr << "An error occurred: " << error.what() << std::endl;
		exitCode = 1;
	}

	curl_global_cleanup();
	return exitCode;
}
