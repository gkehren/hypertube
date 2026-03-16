#include "Theme.hpp"
#include "imgui_internal.h"
#include <cmath>
#include <cstring>

namespace HypertubeTheme
{
	// Static color palette instance
	static ColorPalette s_currentPalette;

	const ColorPalette &getCurrentPalette()
	{
		return s_currentPalette;
	}

	static void applyPaletteToStyle(const ColorPalette &palette)
	{
		ImGuiStyle &style = ImGui::GetStyle();
		ImVec4 *colors = style.Colors;

		colors[ImGuiCol_Text] = palette.textPrimary;
		colors[ImGuiCol_TextDisabled] = palette.textDisabled;
		colors[ImGuiCol_WindowBg] = palette.background;
		colors[ImGuiCol_ChildBg] = palette.backgroundDark;
		colors[ImGuiCol_PopupBg] = palette.popupBg;
		colors[ImGuiCol_Border] = palette.border;
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_FrameBg] = palette.surface;
		colors[ImGuiCol_FrameBgHovered] = palette.surfaceHover;
		colors[ImGuiCol_FrameBgActive] = palette.surfaceActive;
		colors[ImGuiCol_TitleBg] = palette.backgroundDark;
		colors[ImGuiCol_TitleBgActive] = palette.surface;
		colors[ImGuiCol_TitleBgCollapsed] = palette.backgroundDark;
		colors[ImGuiCol_MenuBarBg] = palette.backgroundDark;
		colors[ImGuiCol_ScrollbarBg] = palette.backgroundDark;
		colors[ImGuiCol_ScrollbarGrab] = palette.surface;
		colors[ImGuiCol_ScrollbarGrabHovered] = palette.surfaceHover;
		colors[ImGuiCol_ScrollbarGrabActive] = palette.surfaceActive;
		colors[ImGuiCol_CheckMark] = palette.primary;
		colors[ImGuiCol_SliderGrab] = palette.primary;
		colors[ImGuiCol_SliderGrabActive] = palette.primaryActive;
		colors[ImGuiCol_Button] = palette.surface;
		colors[ImGuiCol_ButtonHovered] = palette.surfaceHover;
		colors[ImGuiCol_ButtonActive] = palette.surfaceActive;
		colors[ImGuiCol_Header] = palette.surface;
		colors[ImGuiCol_HeaderHovered] = palette.surfaceHover;
		colors[ImGuiCol_HeaderActive] = palette.surfaceActive;
		colors[ImGuiCol_Separator] = palette.border;
		colors[ImGuiCol_SeparatorHovered] = palette.primary;
		colors[ImGuiCol_SeparatorActive] = palette.primaryActive;
		colors[ImGuiCol_ResizeGrip] = palette.surface;
		colors[ImGuiCol_ResizeGripHovered] = palette.primary;
		colors[ImGuiCol_ResizeGripActive] = palette.primaryActive;
		colors[ImGuiCol_Tab] = palette.surface;
		colors[ImGuiCol_TabHovered] = palette.primary;
		colors[ImGuiCol_TabActive] = ImVec4(palette.primary.x * 0.7f, palette.primary.y * 0.7f, palette.primary.z * 0.7f, 1.0f);
		colors[ImGuiCol_TabUnfocused] = palette.surface;
		colors[ImGuiCol_TabUnfocusedActive] = palette.surfaceHover;
		colors[ImGuiCol_DockingPreview] = ImVec4(palette.primary.x, palette.primary.y, palette.primary.z, 0.5f);
		colors[ImGuiCol_DockingEmptyBg] = palette.backgroundDark;
		colors[ImGuiCol_PlotLines] = palette.primary;
		colors[ImGuiCol_PlotLinesHovered] = palette.primaryHover;
		colors[ImGuiCol_PlotHistogram] = palette.primary;
		colors[ImGuiCol_PlotHistogramHovered] = palette.primaryHover;
		colors[ImGuiCol_TableHeaderBg] = palette.surface;
		colors[ImGuiCol_TableBorderStrong] = palette.border;
		colors[ImGuiCol_TableBorderLight] = ImVec4(palette.border.x * 0.7f, palette.border.y * 0.7f, palette.border.z * 0.7f, 1.0f);
		colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_TableRowBgAlt] = palette.tableRowBgAlt;
		colors[ImGuiCol_TextSelectedBg] = ImVec4(palette.primary.x, palette.primary.y, palette.primary.z, 0.35f);
		colors[ImGuiCol_DragDropTarget] = palette.primary;
		colors[ImGuiCol_NavHighlight] = palette.primary;
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
	}

	void configureStyleSettings()
	{
		ImGuiStyle &style = ImGui::GetStyle();

		// Window styling
		style.WindowPadding = ImVec2(12.0f, 12.0f);
		style.WindowRounding = 8.0f;
		style.WindowBorderSize = 1.0f;
		style.WindowMinSize = ImVec2(100.0f, 50.0f);
		style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
		style.WindowMenuButtonPosition = ImGuiDir_None;

		// Frame styling
		style.FramePadding = ImVec2(10.0f, 6.0f);
		style.FrameRounding = 6.0f;
		style.FrameBorderSize = 0.0f;

		// Item spacing
		style.ItemSpacing = ImVec2(10.0f, 8.0f);
		style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);

		// Touch/Click area
		style.TouchExtraPadding = ImVec2(0.0f, 0.0f);

		// Indent
		style.IndentSpacing = 22.0f;

		// Scrollbar
		style.ScrollbarSize = 14.0f;
		style.ScrollbarRounding = 6.0f;

		// Grab (sliders, scrollbars)
		style.GrabMinSize = 12.0f;
		style.GrabRounding = 4.0f;

		// Tabs
		style.TabRounding = 6.0f;
		style.TabBorderSize = 0.0f;
		style.TabBarBorderSize = 1.0f;

		// Tables
		style.CellPadding = ImVec2(8.0f, 6.0f);

		// Buttons
		style.ButtonTextAlign = ImVec2(0.5f, 0.5f);

		// Popups
		style.PopupRounding = 8.0f;
		style.PopupBorderSize = 1.0f;

		// Separator
		style.SeparatorTextBorderSize = 2.0f;
		style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
		style.SeparatorTextPadding = ImVec2(20.0f, 3.0f);

		// Child windows
		style.ChildRounding = 6.0f;
		style.ChildBorderSize = 1.0f;

		// Misc
		style.AntiAliasedLines = true;
		style.AntiAliasedFill = true;
		style.AntiAliasedLinesUseTex = true;

		// Alpha settings for disabled
		style.DisabledAlpha = 0.5f;
	}

	void applyTheme(ThemeType theme)
	{
		configureStyleSettings();

		switch (theme)
		{
		case ThemeType::Dark:
			applyModernDarkTheme();
			break;
		case ThemeType::Ocean:
			applyOceanTheme();
			break;
		case ThemeType::Nord:
			applyNordTheme();
			break;
		case ThemeType::Dracula:
			applyDraculaTheme();
			break;
		case ThemeType::CyberPunk:
			applyCyberPunkTheme();
			break;
		default:
			applyModernDarkTheme();
			break;
		}
	}

	void applyModernDarkTheme()
	{
		// Define color palette - Modern dark with teal/cyan accent
		s_currentPalette.primary = ImVec4(0.20f, 0.75f, 0.75f, 1.00f);		// Teal
		s_currentPalette.primaryHover = ImVec4(0.25f, 0.85f, 0.85f, 1.00f); // Lighter teal
		s_currentPalette.primaryActive = ImVec4(0.15f, 0.65f, 0.65f, 1.00f);

		s_currentPalette.accent = ImVec4(0.60f, 0.40f, 0.90f, 1.00f); // Purple accent
		s_currentPalette.accentHover = ImVec4(0.70f, 0.50f, 0.95f, 1.00f);
		s_currentPalette.accentActive = ImVec4(0.50f, 0.30f, 0.80f, 1.00f);

		s_currentPalette.background = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);	  // Very dark
		s_currentPalette.backgroundDark = ImVec4(0.05f, 0.05f, 0.07f, 1.00f); // Darker
		s_currentPalette.backgroundLight = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
		s_currentPalette.popupBg = ImVec4(0.10f, 0.10f, 0.13f, 0.98f);

		s_currentPalette.surface = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
		s_currentPalette.surfaceHover = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
		s_currentPalette.surfaceActive = ImVec4(0.22f, 0.22f, 0.27f, 1.00f);

		s_currentPalette.textPrimary = ImVec4(0.95f, 0.95f, 0.97f, 1.00f);
		s_currentPalette.textSecondary = ImVec4(0.60f, 0.60f, 0.65f, 1.00f);
		s_currentPalette.textDisabled = ImVec4(0.40f, 0.40f, 0.45f, 1.00f);

		s_currentPalette.success = ImVec4(0.30f, 0.80f, 0.50f, 1.00f);
		s_currentPalette.warning = ImVec4(0.95f, 0.75f, 0.25f, 1.00f);
		s_currentPalette.error = ImVec4(0.90f, 0.35f, 0.40f, 1.00f);
		s_currentPalette.info = ImVec4(0.35f, 0.70f, 0.95f, 1.00f);

		s_currentPalette.border = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
		s_currentPalette.borderHover = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);

		s_currentPalette.progressDownload = ImVec4(0.20f, 0.75f, 0.75f, 1.00f);
		s_currentPalette.progressUpload = ImVec4(0.60f, 0.40f, 0.90f, 1.00f);
		s_currentPalette.progressBackground = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);

		s_currentPalette.tableRowBgAlt = ImVec4(0.07f, 0.07f, 0.10f, 0.50f);

		// Apply palette
		applyPaletteToStyle(s_currentPalette);
	}

	void applyOceanTheme()
	{
		// Deep ocean blue palette
		s_currentPalette.primary = ImVec4(0.30f, 0.60f, 0.90f, 1.00f);
		s_currentPalette.primaryHover = ImVec4(0.40f, 0.70f, 0.95f, 1.00f);
		s_currentPalette.primaryActive = ImVec4(0.25f, 0.50f, 0.80f, 1.00f);

		s_currentPalette.accent = ImVec4(0.40f, 0.80f, 0.80f, 1.00f);
		s_currentPalette.accentHover = ImVec4(0.50f, 0.90f, 0.90f, 1.00f);
		s_currentPalette.accentActive = ImVec4(0.30f, 0.70f, 0.70f, 1.00f);

		s_currentPalette.background = ImVec4(0.06f, 0.10f, 0.15f, 1.00f);
		s_currentPalette.backgroundDark = ImVec4(0.04f, 0.07f, 0.12f, 1.00f);
		s_currentPalette.backgroundLight = ImVec4(0.10f, 0.15f, 0.20f, 1.00f);
		s_currentPalette.popupBg = ImVec4(0.08f, 0.12f, 0.18f, 0.98f);

		s_currentPalette.surface = ImVec4(0.10f, 0.16f, 0.22f, 1.00f);
		s_currentPalette.surfaceHover = ImVec4(0.14f, 0.20f, 0.28f, 1.00f);
		s_currentPalette.surfaceActive = ImVec4(0.18f, 0.25f, 0.33f, 1.00f);

		s_currentPalette.textPrimary = ImVec4(0.90f, 0.95f, 0.98f, 1.00f);
		s_currentPalette.textSecondary = ImVec4(0.55f, 0.65f, 0.75f, 1.00f);
		s_currentPalette.textDisabled = ImVec4(0.40f, 0.45f, 0.50f, 1.00f);

		s_currentPalette.success = ImVec4(0.30f, 0.80f, 0.60f, 1.00f);
		s_currentPalette.warning = ImVec4(0.95f, 0.75f, 0.30f, 1.00f);
		s_currentPalette.error = ImVec4(0.85f, 0.35f, 0.45f, 1.00f);
		s_currentPalette.info = s_currentPalette.primary;

		s_currentPalette.border = ImVec4(0.18f, 0.25f, 0.35f, 1.00f);
		s_currentPalette.borderHover = ImVec4(0.28f, 0.38f, 0.50f, 1.00f);

		s_currentPalette.progressDownload = s_currentPalette.primary;
		s_currentPalette.progressUpload = s_currentPalette.accent;
		s_currentPalette.progressBackground = s_currentPalette.surface;

		s_currentPalette.tableRowBgAlt = ImVec4(0.05f, 0.08f, 0.12f, 0.50f);

		// Apply palette
		applyPaletteToStyle(s_currentPalette);
	}

	void applyNordTheme()
	{
		// Nord color palette
		s_currentPalette.primary = ImVec4(0.53f, 0.75f, 0.82f, 1.00f);		// Nord8
		s_currentPalette.primaryHover = ImVec4(0.58f, 0.80f, 0.87f, 1.00f); // Lighter Nord8
		s_currentPalette.primaryActive = ImVec4(0.48f, 0.70f, 0.77f, 1.00f);

		s_currentPalette.accent = ImVec4(0.70f, 0.56f, 0.68f, 1.00f); // Nord15
		s_currentPalette.accentHover = ImVec4(0.75f, 0.61f, 0.73f, 1.00f);
		s_currentPalette.accentActive = ImVec4(0.65f, 0.51f, 0.63f, 1.00f);

		s_currentPalette.background = ImVec4(0.18f, 0.20f, 0.25f, 1.00f);	  // Nord0
		s_currentPalette.backgroundDark = ImVec4(0.15f, 0.17f, 0.21f, 1.00f); // Darker Nord0
		s_currentPalette.backgroundLight = ImVec4(0.23f, 0.26f, 0.32f, 1.00f);
		s_currentPalette.popupBg = ImVec4(0.20f, 0.22f, 0.28f, 0.98f);

		s_currentPalette.surface = ImVec4(0.23f, 0.26f, 0.32f, 1.00f); // Nord1
		s_currentPalette.surfaceHover = ImVec4(0.26f, 0.30f, 0.37f, 1.00f);
		s_currentPalette.surfaceActive = ImVec4(0.30f, 0.34f, 0.42f, 1.00f);

		s_currentPalette.textPrimary = ImVec4(0.93f, 0.94f, 0.96f, 1.00f);	 // Nord6
		s_currentPalette.textSecondary = ImVec4(0.82f, 0.85f, 0.89f, 1.00f); // Nord5
		s_currentPalette.textDisabled = ImVec4(0.56f, 0.60f, 0.67f, 1.00f);

		s_currentPalette.success = ImVec4(0.64f, 0.75f, 0.55f, 1.00f); // Nord14
		s_currentPalette.warning = ImVec4(0.92f, 0.80f, 0.55f, 1.00f); // Nord13
		s_currentPalette.error = ImVec4(0.75f, 0.38f, 0.42f, 1.00f);   // Nord11
		s_currentPalette.info = s_currentPalette.primary;

		s_currentPalette.border = ImVec4(0.30f, 0.34f, 0.42f, 1.00f);
		s_currentPalette.borderHover = ImVec4(0.40f, 0.44f, 0.52f, 1.00f);

		s_currentPalette.progressDownload = s_currentPalette.primary;
		s_currentPalette.progressUpload = s_currentPalette.accent;
		s_currentPalette.progressBackground = s_currentPalette.surface;

		s_currentPalette.tableRowBgAlt = ImVec4(0.20f, 0.22f, 0.27f, 0.40f);

		// Apply palette
		applyPaletteToStyle(s_currentPalette);
	}

	void applyDraculaTheme()
	{
		// Dracula color palette
		s_currentPalette.primary = ImVec4(0.74f, 0.58f, 0.98f, 1.00f);		// Purple
		s_currentPalette.primaryHover = ImVec4(0.80f, 0.64f, 1.00f, 1.00f); // Lighter purple
		s_currentPalette.primaryActive = ImVec4(0.68f, 0.52f, 0.92f, 1.00f);

		s_currentPalette.accent = ImVec4(1.00f, 0.47f, 0.66f, 1.00f); // Pink
		s_currentPalette.accentHover = ImVec4(1.00f, 0.55f, 0.72f, 1.00f);
		s_currentPalette.accentActive = ImVec4(0.94f, 0.41f, 0.60f, 1.00f);

		s_currentPalette.background = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);	  // Background
		s_currentPalette.backgroundDark = ImVec4(0.11f, 0.11f, 0.15f, 1.00f); // Darker
		s_currentPalette.backgroundLight = ImVec4(0.21f, 0.21f, 0.27f, 1.00f);
		s_currentPalette.popupBg = ImVec4(0.18f, 0.18f, 0.24f, 0.98f);

		s_currentPalette.surface = ImVec4(0.27f, 0.28f, 0.35f, 1.00f); // Current line
		s_currentPalette.surfaceHover = ImVec4(0.32f, 0.33f, 0.42f, 1.00f);
		s_currentPalette.surfaceActive = ImVec4(0.37f, 0.38f, 0.48f, 1.00f);

		s_currentPalette.textPrimary = ImVec4(0.97f, 0.97f, 0.95f, 1.00f);	 // Foreground
		s_currentPalette.textSecondary = ImVec4(0.38f, 0.45f, 0.53f, 1.00f); // Comment
		s_currentPalette.textDisabled = ImVec4(0.38f, 0.40f, 0.48f, 1.00f);

		s_currentPalette.success = ImVec4(0.31f, 0.98f, 0.48f, 1.00f); // Green
		s_currentPalette.warning = ImVec4(1.00f, 0.72f, 0.42f, 1.00f); // Orange
		s_currentPalette.error = ImVec4(1.00f, 0.33f, 0.33f, 1.00f);   // Red
		s_currentPalette.info = ImVec4(0.55f, 0.86f, 0.99f, 1.00f);	   // Cyan

		s_currentPalette.border = ImVec4(0.37f, 0.38f, 0.48f, 1.00f);
		s_currentPalette.borderHover = ImVec4(0.47f, 0.48f, 0.58f, 1.00f);

		s_currentPalette.progressDownload = s_currentPalette.info;
		s_currentPalette.progressUpload = s_currentPalette.primary;
		s_currentPalette.progressBackground = s_currentPalette.surface;

		s_currentPalette.tableRowBgAlt = ImVec4(0.20f, 0.20f, 0.26f, 0.40f);

		// Apply palette
		applyPaletteToStyle(s_currentPalette);
	}

	void applyCyberPunkTheme()
	{
		// Cyberpunk color palette - neon yellow/pink on dark
		s_currentPalette.primary = ImVec4(1.00f, 0.96f, 0.00f, 1.00f);		// Neon yellow
		s_currentPalette.primaryHover = ImVec4(1.00f, 1.00f, 0.30f, 1.00f); // Brighter yellow
		s_currentPalette.primaryActive = ImVec4(0.85f, 0.82f, 0.00f, 1.00f);

		s_currentPalette.accent = ImVec4(1.00f, 0.00f, 0.50f, 1.00f); // Neon pink/magenta
		s_currentPalette.accentHover = ImVec4(1.00f, 0.20f, 0.60f, 1.00f);
		s_currentPalette.accentActive = ImVec4(0.85f, 0.00f, 0.42f, 1.00f);

		s_currentPalette.background = ImVec4(0.04f, 0.04f, 0.06f, 1.00f);	  // Near black
		s_currentPalette.backgroundDark = ImVec4(0.02f, 0.02f, 0.04f, 1.00f); // Darker
		s_currentPalette.backgroundLight = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
		s_currentPalette.popupBg = ImVec4(0.06f, 0.06f, 0.09f, 0.98f);

		s_currentPalette.surface = ImVec4(0.10f, 0.10f, 0.14f, 1.00f);
		s_currentPalette.surfaceHover = ImVec4(0.14f, 0.14f, 0.20f, 1.00f);
		s_currentPalette.surfaceActive = ImVec4(0.18f, 0.18f, 0.25f, 1.00f);

		s_currentPalette.textPrimary = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
		s_currentPalette.textSecondary = ImVec4(0.60f, 0.60f, 0.65f, 1.00f);
		s_currentPalette.textDisabled = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);

		s_currentPalette.success = ImVec4(0.00f, 1.00f, 0.60f, 1.00f); // Neon green
		s_currentPalette.warning = s_currentPalette.primary;
		s_currentPalette.error = s_currentPalette.accent;
		s_currentPalette.info = ImVec4(0.00f, 0.85f, 1.00f, 1.00f); // Neon cyan

		s_currentPalette.border = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
		s_currentPalette.borderHover = s_currentPalette.primary;

		s_currentPalette.progressDownload = s_currentPalette.info;
		s_currentPalette.progressUpload = s_currentPalette.accent;
		s_currentPalette.progressBackground = s_currentPalette.surface;

		s_currentPalette.tableRowBgAlt = ImVec4(0.06f, 0.06f, 0.08f, 0.50f);

		// Apply palette
		applyPaletteToStyle(s_currentPalette);

		// Cyberpunk-specific overrides
		ImGuiStyle &style = ImGui::GetStyle();
		style.Colors[ImGuiCol_ScrollbarGrabActive] = s_currentPalette.primary;
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(s_currentPalette.primary.x * 0.2f, s_currentPalette.primary.y * 0.2f, s_currentPalette.primary.z * 0.2f, 1.0f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(s_currentPalette.primary.x * 0.3f, s_currentPalette.primary.y * 0.3f, s_currentPalette.primary.z * 0.3f, 1.0f);
		style.Colors[ImGuiCol_TabHovered] = ImVec4(s_currentPalette.primary.x * 0.3f, s_currentPalette.primary.y * 0.3f, s_currentPalette.primary.z * 0.3f, 1.0f);
		style.Colors[ImGuiCol_TabActive] = ImVec4(s_currentPalette.primary.x * 0.2f, s_currentPalette.primary.y * 0.2f, s_currentPalette.primary.z * 0.2f, 1.0f);
		style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.70f);
	}

	void configureFonts(ImGuiIO &io, float fontSize)
	{
		// Use the default font with adjusted size
		// In production, you'd load a custom font here like:
		// io.Fonts->AddFontFromFileTTF("path/to/font.ttf", fontSize);
		ImFontConfig fontConfig;
		fontConfig.OversampleH = 3;
		fontConfig.OversampleV = 2;
		fontConfig.PixelSnapH = true;

		io.Fonts->AddFontDefault(&fontConfig);
	}

	// Custom widget implementations
	bool drawStyledButton(const char *label, const ImVec2 &size, bool isPrimary)
	{
		if (isPrimary)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, s_currentPalette.primary);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, s_currentPalette.primaryHover);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, s_currentPalette.primaryActive);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Dark text on bright button
		}

		bool result = ImGui::Button(label, size);

		if (isPrimary)
		{
			ImGui::PopStyleColor(4);
		}

		return result;
	}

	void drawSectionHeader(const char *label)
	{
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, s_currentPalette.primary);
		ImGui::Text("%s", label);
		ImGui::PopStyleColor();
		ImGui::Separator();
		ImGui::Spacing();
	}

	void drawTooltip(const char *text)
	{
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
			ImGui::TextUnformatted(text);
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	bool drawCategoryItem(const char *label, const char *icon, bool selected, int count)
	{
		ImGuiWindow *window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		ImGuiContext &g = *GImGui;
		const ImGuiStyle &style = g.Style;

		ImVec2 pos = window->DC.CursorPos;
		ImVec2 size(ImGui::GetContentRegionAvail().x, g.FontSize + style.FramePadding.y * 2.0f);
		ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

		ImGui::ItemSize(bb, style.FramePadding.y);
		ImGuiID id = window->GetID(label);
		if (!ImGui::ItemAdd(bb, id))
			return false;

		bool hovered, held;
		bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

		// Draw background
		ImVec4 bgColor = selected ? s_currentPalette.surfaceActive : (hovered ? s_currentPalette.surfaceHover : ImVec4(0, 0, 0, 0));
		if (selected || hovered)
		{
			window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(bgColor), style.FrameRounding);
		}

		// Draw selection indicator
		if (selected)
		{
			ImRect indicatorRect(bb.Min, ImVec2(bb.Min.x + 3.0f, bb.Max.y));
			window->DrawList->AddRectFilled(indicatorRect.Min, indicatorRect.Max, ImGui::GetColorU32(s_currentPalette.primary), 2.0f);
		}

		// Draw icon placeholder and label
		float textX = bb.Min.x + style.FramePadding.x + (selected ? 8.0f : 4.0f);
		ImVec4 textColor = selected ? s_currentPalette.textPrimary : (hovered ? s_currentPalette.textPrimary : s_currentPalette.textSecondary);
		window->DrawList->AddText(ImVec2(textX, bb.Min.y + style.FramePadding.y), ImGui::GetColorU32(textColor), label);

		// Draw count badge if provided
		if (count >= 0)
		{
			char countStr[16];
			snprintf(countStr, sizeof(countStr), "%d", count);
			ImVec2 countSize = ImGui::CalcTextSize(countStr);
			float countX = bb.Max.x - style.FramePadding.x - countSize.x;
			window->DrawList->AddText(ImVec2(countX, bb.Min.y + style.FramePadding.y), ImGui::GetColorU32(s_currentPalette.textDisabled), countStr);
		}

		return pressed;
	}

	ImVec4 getStatusColor(const char *status)
	{
		if (strcmp(status, "Downloading") == 0)
			return s_currentPalette.info;
		if (strcmp(status, "Seeding") == 0)
			return s_currentPalette.success;
		if (strcmp(status, "Completed") == 0)
			return s_currentPalette.success;
		if (strcmp(status, "Paused") == 0)
			return s_currentPalette.warning;
		if (strcmp(status, "Error") == 0)
			return s_currentPalette.error;
		if (strcmp(status, "Checking") == 0)
			return s_currentPalette.info;
		if (strcmp(status, "Queued") == 0)
			return s_currentPalette.textSecondary;
		return s_currentPalette.textPrimary;
	}

	ImVec4 getHealthColor(float seedRatio)
	{
		if (seedRatio >= 2.0f)
			return s_currentPalette.success;
		if (seedRatio >= 1.0f)
			return ImVec4(0.70f, 0.85f, 0.40f, 1.0f); // Yellow-green
		if (seedRatio >= 0.5f)
			return s_currentPalette.warning;
		return s_currentPalette.error;
	}

	float pulse(float speed)
	{
		return (sinf((float)ImGui::GetTime() * speed) + 1.0f) * 0.5f;
	}

	ImVec4 lerpColor(const ImVec4 &a, const ImVec4 &b, float t)
	{
		return ImVec4(
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t,
			a.w + (b.w - a.w) * t);
	}
}
