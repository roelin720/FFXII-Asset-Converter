#pragma once
#include "ctpl_stl.h"
#include "PhyreGUILogs.h"
#include "PhyreGUIPathHistory.h"
#include "PhyreGUIBrowser.h"
#include "PhyreGUITexture.h"
#include <map>
#include "imgui.h"

#define LOG(type, message) std::cout << message << std::endl; Phyre::GUI::log_mutex.lock(); logs.push(Phyre::Log(type, message)); Phyre::GUI::log_mutex.unlock();

namespace Phyre
{
	class GUI
	{
	public:
		static constexpr int thread_count_max = 6;

		static std::mutex log_mutex;
		static std::mutex mute_mutex;
		static ctpl::thread_pool thread_pool;

		static Phyre::Logs logs;
		static Phyre::PathHistory history;
		static Phyre::FileBrowser browser;

		GUITexture folder_icon;
		GUITexture file_icon;
		GUITexture play_icon;
		GUITexture play_file_icon;
		GUITexture play_muted_icon;
		GUITexture play_unmuted_icon;

		std::string play_path;
		bool play_muted;

		std::thread mute_thread;

		bool exiting = false;
		bool processing = false;
		
		Phyre::PathID dialog_pathID = Phyre::PathID_INVALID;

		GUI();

		bool run();
		void draw_ui();

		bool draw_file_dialog();

		void load_icon_textures();
		void free_icon_textures();

		void load_play_config();
		void save_play_config();
		void draw_play_buttons();

		bool draw_processing_button(std::string name);
		bool draw_input_path(const char* label, char* buf, PathID pathID);

		bool run_command(bool pack, bool unpack);
		bool aquire_confirmation(bool pack, bool unpack);
		bool validate_paths(bool pack, bool unpack);
	};
}