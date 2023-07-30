#pragma once
#include "ctpl_stl.h"
#include "GUILogs.h"
#include "GUIPathHistory.h"
#include "GUIIcon.h"
#include "VBFArchive.h"
#include <vector>
#include <string>
#include "imgui.h"
#include "imgui_internal.h"
#include <functional>
#include <Windows.h>
#include "ClipDropTarget.h"

class GUI;
class FileBrowser
{
private:
	struct generator
	{
		uint8_t* begin = nullptr;
		uint8_t* end = nullptr;
		size_t stride = 0;

		template<typename VectorT>
		static inline generator create(const VectorT& vec)
		{
			generator g;
			if (!vec.empty())
			{
				g.begin = (uint8_t*)(vec.data());
				g.end = (uint8_t*)(vec.data() + vec.size());
				g.stride = sizeof(VectorT::value_type);
			}
			return g;
		}

		inline void* next()
		{
			if (begin >= end) return nullptr;
			uint8_t* value_ptr = begin;
			begin += stride;
			return value_ptr;
		}
	};

public:
	enum DialogCode
	{
		DialogCode_None,
		DialogCode_Confirm,
		DialogCode_Cancel
	};
	struct ArchiveRecord
	{
		VBFArchive archive;
		bool loaded = false;
		bool valid = false;
	};
	std::vector<std::string> cur_parent_folder;
	std::string cur_filename;
	std::vector<std::string> drives;
	std::map<std::string, ArchiveRecord> archives;
	bool focus_current_folder = true;
	bool require_existing_file = false;
	bool confirm_overwrite = false;

	std::vector<std::string> pinned_folders;
	std::vector<std::vector<std::string>> folder_history;
	std::vector<std::vector<std::string>>::iterator folder_history_end;

	std::vector<std::pair<std::string, std::vector<std::string>>> filters = { {"All files", {"*"}} };
	size_t filter_index = 0;

	std::vector<std::pair<float, std::function<void(float)>>> popup_messages;

	GUI& gui;

	HWND window = NULL;
	ClipDropTarget* drop_target = nullptr;
	bool dragging_current_file = false;
	bool dragging_left = false;

	bool renaming_current_file = false;

	GUIIcon folder_icon;
	GUIIcon file_icon;
	GUIIcon archive_icon;
	GUIIcon model_icon;
	GUIIcon image_icon;
	GUIIcon music_icon;
	GUIIcon text_icon;
	GUIIcon video_icon;
	GUIIcon exe_icon;
	GUIIcon dll_icon;
	GUIIcon pin_icon;
	GUIIcon left_icon;
	GUIIcon right_icon;
	GUIIcon up_icon;
	GUIIcon down_icon;
	GUIIcon left_icon_tiny;
	GUIIcon right_icon_tiny;

	DialogCode dialog_code = DialogCode_None;

	FileBrowser(GUI& gui);

	bool init(HWND window);
	void free();

	bool set_current_path(const std::string& path);
	DialogCode run_dialog();

	void draw_popup_messages();
	void popup_error(const std::string& msg);
	void popup_info(const std::string& msg);

	bool valid(const std::string& path);
	bool valid(const std::vector<std::string>& path);

	ArchiveRecord& get_vbf(const std::string& vbf_path);

	void drag_current_file();
	void create_new_folder();
	void rename_current_file(const std::string& new_name);
	void trash_current_file();
	
	void draw_content();
	void draw_rename_modal_window();
	void draw_archive_modal_window();
	void draw_current_folder_pane();
	void draw_folder_tree_pane();
	void draw_quick_access_pane();
	void draw_nav_bar();
	bool draw_footer();

	void load_pinned_folders();
	void push_pinned_folder(const std::vector<std::string>& folder);
	void erase_pinned_folder(size_t index);
	void save_pinned_folders();

	void set_current_folder(const std::vector<std::string>& folder);
	void set_current_folder_to_prev();
	void set_current_folder_to_next();

	ImRect draw_tree(const std::vector<std::string>& path);
	ImRect draw_vbf_tree(const std::vector<std::string>& vbf_path, const std::string& vbf_path_str, const std::string& vbf_name);
	ImRect draw_vbf_tree(const std::vector<std::string>& vbf_path, const std::vector<std::string>& cur_arc_name, const std::vector<std::string>& arc_name, const VBFArchive::TreeNode& node);

	ImRect draw_folder_tree_node(const std::vector<std::string>& path, bool& clicked, bool selected, bool leaf, const std::function<void()> pre_children, generator child_generator, const std::function<ImRect(void*)>& child_operator, int type_code = 0);
};

