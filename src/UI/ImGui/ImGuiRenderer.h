/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#ifndef IMGUI_RENDERER_H
#define IMGUI_RENDERER_H

#include "Renderer/OpenImageDenoiser.h"
#include "UI/ApplicationSettings.h"
#include "UI/ImGui/ImGuiRendererPerformancePreset.h"
#include "UI/ImGui/ImGuiRenderWindow.h"
#include "UI/ImGui/ImGuiSettingsWindow.h"
#include "UI/PerformanceMetricsComputer.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <memory>

class GPURenderer;
class RenderWindow;

class ImGuiRenderer
{
public:
	ImGuiRenderer();
	static void init_imgui(GLFWwindow* glfw_window);

	/**
  	 * Adds a tooltip to the last widget that auto wraps after 80 characters
	 */
	static void wrapping_tooltip(const std::string& text);
	static void add_tooltip(const std::string& tooltip_text, ImGuiHoveredFlags flags = ImGuiHoveredFlags_AllowWhenDisabled);
	static void show_help_marker(const std::string& text);

	void set_render_window(RenderWindow* renderer);

	void draw_interface();
	void rescale_ui();
	void draw_dockspace();

	void draw_settings_window();
	void draw_render_window();

	int get_render_viewport_width();
	int get_render_viewport_height();

private:
	ImGuiSettingsWindow m_imgui_settings_window;
	ImGuiRenderWindow m_imgui_render_window;

	RenderWindow* m_render_window = nullptr;
};

#endif
