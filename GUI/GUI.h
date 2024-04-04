#pragma once
#include "Enums.h"
#include "ctpl_stl.h"
#include "GUILogs.h"
#include "GUIPathHistory.h"
#include "GUIBrowser.h"
#include "GUIIcon.h"
#include <map>
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "imgui.h"
#include "Pipe.h"

#define GUILOG(type, message) { std::cout << message << std::endl; GUI::log_mutex.lock(); logs.push(::GUILog(type, message)); GUI::log_mutex.unlock(); }

namespace GUIMessagePayload
{
	struct WindowInit
	{
		static constexpr size_t title_max_len = 128;
		char title[title_max_len] = {};
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t pos_x = 0;
		uint32_t pos_y = 0;
		std::pair<int, int> hints[4] = {};
		HWND parent_window = NULL;
	};
	struct Generic
	{
		static constexpr size_t label_max_len = 1024;
		char label[label_max_len] = {};
	};
	struct Progress : public Generic
	{
		uint32_t elpased = 0;
		uint32_t destination = 0;
		bool cancel_disabled = false;
	};
	struct Confirmation : public Generic
	{
		GUIMessage button_types = GUIMessage::INVALID;
	};
}

class GUI
{
public:
	static constexpr int thread_count_max = 6;

	static std::recursive_mutex log_mutex;
	static std::mutex mute_mutex;
	static ctpl::thread_pool thread_pool;

	static GUILogs logs;
	static PathHistory history;

	FileBrowser browser;

	GUITask task = GUITask::Main;
	PipeClient pipe;
	GUIMessage last_post = GUIMessage::INVALID;

	GLFWwindow* window = nullptr;
	HANDLE window_mutex = NULL;

	GUIIcon folder_icon;
	GUIIcon file_icon;
	GUIIcon play_icon;
	GUIIcon play_file_icon;
	GUIIcon play_muted_icon;
	GUIIcon play_unmuted_icon;
	GUIIcon visibility_icon;
	GUIIcon copy_icon;
	GUIIcon save_icon;

	std::string config_path;
	bool play_muted;

	std::thread mute_thread;

	bool exiting = false;
	bool processing = false;
	bool info_visible = true;
	bool warnings_visible = true;
	bool convert_via_cmd_interface = false;

	PathID dialog_pathID = PathID_INVALID;

	static int32_t last_char;

	GUI();

	bool init(GUITask task = GUITask::Main, std::string pipe_name = "");
	void run();
	void free();
	void draw_main_ui();
	void draw_confirmation_ui();
	void draw_progress_ui();

	void draw_debug_widgets();

	GUIMessage confirm(const std::string& label, GUIMessage button_types);
	bool confirm(const std::string& label);
	void alert(const std::string& label);

	bool draw_file_dialog();

	void load_icon_textures();
	void free_icon_textures();

	void load_config();
	void save_config();

	void draw_play_buttons();

	bool draw_processing_button(std::string name);
	bool draw_input_path(const char* label, char* buf, PathID pathID);

	bool run_command(bool pack, bool unpack);
	bool validate_paths(bool pack, bool unpack);
	
	bool is_log_visible(const GUILog& log);
};
