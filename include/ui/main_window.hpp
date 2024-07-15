#pragma once

#include "ui.hpp"
#include <stdexcept>
#include <iostream>

class MainWindow
{
	public:
		MainWindow();
		~MainWindow();
		void show();

	private:
		GLFWwindow*	window;
		UI			ui;
};
