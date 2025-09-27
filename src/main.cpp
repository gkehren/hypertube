#include <iostream>
#include "App.hpp"

int main(int, char **)
{
	try
	{
		App app;
		app.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << "An error occured: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}
