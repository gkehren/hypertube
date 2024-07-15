#include <iostream>
#include "app.hpp"

int	main() {
	try {
		App	app;
		app.run();
	} catch (const std::exception& e) {
		std::cerr << "An error occured: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
