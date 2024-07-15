#pragma once

#include "ui.hpp"
#include <stdexcept>

class MainWindow {
	public:
		MainWindow();
		~MainWindow();
		void show();

	private:
		GLFWwindow*	window;
		UI			ui;
};
