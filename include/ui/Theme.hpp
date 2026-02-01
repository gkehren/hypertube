#pragma once

#include <imgui.h>
#include <string>

namespace HypertubeTheme
{
	// Color palette
	struct ColorPalette
	{
		// Primary colors
		ImVec4 primary;
		ImVec4 primaryHover;
		ImVec4 primaryActive;

		// Accent colors
		ImVec4 accent;
		ImVec4 accentHover;
		ImVec4 accentActive;

		// Background colors
		ImVec4 background;
		ImVec4 backgroundDark;
		ImVec4 backgroundLight;

		// Surface colors
		ImVec4 surface;
		ImVec4 surfaceHover;
		ImVec4 surfaceActive;

		// Text colors
		ImVec4 textPrimary;
		ImVec4 textSecondary;
		ImVec4 textDisabled;

		// Status colors
		ImVec4 success;
		ImVec4 warning;
		ImVec4 error;
		ImVec4 info;

		// Border colors
		ImVec4 border;
		ImVec4 borderHover;

		// Progress bar colors
		ImVec4 progressDownload;
		ImVec4 progressUpload;
		ImVec4 progressBackground;
	};

	// Pre-defined themes
	enum class ThemeType
	{
		Dark,
		Ocean,
		Nord,
		Dracula,
		CyberPunk
	};

	// Theme configuration
	void applyTheme(ThemeType theme = ThemeType::Dark);
	void applyModernDarkTheme();
	void applyOceanTheme();
	void applyNordTheme();
	void applyDraculaTheme();
	void applyCyberPunkTheme();

	// Style configuration
	void configureStyleSettings();
	void configureFonts(ImGuiIO &io, float fontSize = 15.0f);

	// Get current color palette
	const ColorPalette &getCurrentPalette();

	// Custom styled widgets
	void drawProgressBarColored(float fraction, const ImVec4 &color, const ImVec2 &size = ImVec2(-1, 0));
	void drawStatusBadge(const char *label, const ImVec4 &color);
	bool drawStyledButton(const char *label, const ImVec2 &size = ImVec2(0, 0), bool isPrimary = false);
	void drawSectionHeader(const char *label);
	void drawTooltip(const char *text);
	void drawSearchBar(const char *label, char *buffer, size_t bufferSize, bool *enterPressed = nullptr);

	// Category sidebar item
	bool drawCategoryItem(const char *label, const char *icon, bool selected, int count = -1);

	// Torrent status helpers
	ImVec4 getStatusColor(const char *status);
	ImVec4 getHealthColor(float seedRatio);

	// Animation helpers
	float pulse(float speed = 1.0f);
	ImVec4 lerpColor(const ImVec4 &a, const ImVec4 &b, float t);
}
