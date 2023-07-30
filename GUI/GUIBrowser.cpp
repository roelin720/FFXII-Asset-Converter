#include "GUIBrowser.h"
#include "ConverterInterface.h"
#include "GUI.h"
#include "FileIOUtils.h"
#include <iostream>
#include <Shlwapi.h>
#include <filesystem>
#include <shared_mutex>
#include "Icons/file_icon_small_data.h"
#include "Icons/folder_icon_small_data.h"
#include "Icons/archive_icon_small_data.h"
#include "Icons/model_icon_small_data.h"
#include "Icons/image_icon_small_data.h"
#include "Icons/music_icon_small_data.h"
#include "Icons/text_icon_small_data.h"
#include "Icons/video_icon_small_data.h"
#include "Icons/exe_icon_small_data.h"
#include "Icons/dll_icon_small_data.h"
#include "Icons/pin_icon_small_data.h"
#include "Icons/left_icon_data.h"
#include "Icons/right_icon_data.h"
#include "Icons/up_icon_data.h"
#include "Icons/down_icon_data.h"
#include "Icons/left_icon_tiny_data.h"
#include "Icons/right_icon_tiny_data.h"
#include <sys/stat.h>
#include <shlobj.h>
#include <regex>
#include <sstream>
#include <string>
#include "ClipDragDrop.h"
#include "ClipDropTarget.h"
#include <ShlObj.h>

namespace
{
	struct FileInfo
	{
		enum TypeID
		{
			File		 = 1 << 0,
			Folder		 = 1 << 1,
			Image		 = 1 << 2,
			Model		 = 1 << 3,
			Archive		 = 1 << 4,
			VBFArchive	 = 1 << 5,
			Music		 = 1 << 6,
			Text		 = 1 << 7,
			Video		 = 1 << 8,
			Exe			 = 1 << 9,
			Dll			 = 1 << 10
		};
		enum FieldID
		{
			Name			= 1 << 0,
			Size			= 1 << 1,
			DateModified	= 1 << 2,
			DateCreated		= 1 << 3,
			Type			= 1 << 4,
		};
		std::string name;
		std::string size_str;
		std::string date_modified_str;
		std::string date_created_str;
		std::string type_str;

		TypeID typeID = TypeID::File;
		int64_t size = 0;
		int64_t date_modified = 0;
		int64_t date_created = 0;
	};

	std::shared_mutex file_mutex;
	std::map<std::string, std::vector<FileInfo>> file_children_map;
	std::map<std::string, bool> file_processing_map;

	std::shared_mutex folder_mutex;
	std::map<std::string, std::vector<std::string>> folder_children_map;
	std::map<std::string, bool> folder_processing_map;

	FileInfo::TypeID get_type_id(const std::string& ext)
	{
		#define _ || ext ==
		if (0 _"png" _"gif" _"jpg" _"jpeg" _"jfif" _"pjpeg" _"pjp" _"png" _"svg" _"tga" _"dds" _"dds.phyre" _"ico")
		{
			return FileInfo::TypeID::Image;
		}
		if (0 _"zip" _"rar")
		{
			return FileInfo::TypeID::Archive;
		}
		if (0 _"vbf")
		{
			return FileInfo::TypeID::VBFArchive;
		}
		if (0 _"stp" _"max" _"fbx" _"obj" _"x3d" _"vrml" _"3ds" _"3mf" _"stl" _"dae" _"dae.phyre")
		{
			return FileInfo::TypeID::Model;
		}
		if (0 _"ah" _"txt" _"text" _"config" _"cfg" _"json" _"ini" _"xml" _"yaml" _"h" _"hpp" _"c" _"cpp" _"cs" _"java")
		{
			return FileInfo::TypeID::Text;
		}
		if (0 _"win.mab" _"win.sab" _"vpc" _"vpcjp" _"m4a" _"flac" _"mp3" _"wav" _"wma" _"aac" _"ogg")
		{
			return FileInfo::TypeID::Music;
		}
		if (0 _"webm" _"mp4" _"mov" _"wmv" _"avi" _"mkv" _"flv" _"webm" _"wma" _"mpeg")
		{
			return FileInfo::TypeID::Video;
		}
		if (0 _"exe" _"jar" _"py")
		{
			return FileInfo::TypeID::Exe;
		}
		if (0 _"dll" _"lib" _"cdx")
		{
			return FileInfo::TypeID::Dll;
		}
		return FileInfo::TypeID::File;
		#undef _
	}

	bool is_wide(const std::filesystem::path& path)
	{
		for (auto ch : path.wstring())
		{
			if (ch > 127)
			{
				return true;
			}
		}
		return false;
	}

	void compare(const std::vector<std::string>& a, const std::vector<std::string>& b, bool& in_b, bool& equals_b)
	{
		in_b = !b.empty();
		equals_b = false;

		if (a.size() <= b.size())
		{
			for (int i = 0; i < a.size(); ++i)
			{
				if (strlen(a[i].c_str()) != strlen(b[i].c_str()))
				{
					in_b = false;
					break;
				}
				for (int j = 0; j < a[i].size(); ++j)
				{
					if (std::tolower(a[i][j]) != std::tolower(b[i][j]))
					{
						in_b = false;
						break;
					}
				}
			}
		}
		else
		{
			in_b = false;
		}
		equals_b = in_b && (a.size() == b.size());
	}

	bool compare(const std::string& a, const std::string& b)
	{
		if (strlen(a.c_str()) != strlen(b.c_str()))
		{
			return false;
		}
		for (int i = 0; i < a.size(); ++i)
		{
			if (std::tolower(a[i]) != std::tolower(b[i]))
			{
				return false;
			}
		}

		return true;
	}

	std::vector<FileInfo> get_child_files(const std::string& folder_str, const std::vector<std::string>& filter, bool& is_new, const std::vector<ImGuiTableColumnSortSpecs>& sort_specs, const FileBrowser::ArchiveRecord& arc_rec)
	{
		std::string map_key = arc_rec.archive.arc_path + "/" + folder_str;

		decltype(file_children_map)::iterator it;
		file_mutex.lock_shared();
		it = file_children_map.find(map_key);
		file_mutex.unlock_shared();
		is_new = it == file_children_map.end();

		if (bool& processing = file_processing_map.insert({ folder_str, false }).first->second; !processing)
		{
			processing = true;
			GUI::thread_pool.push([&processing, filter, folder_str, sort_specs, &arc_rec, map_key](int)
			{
				std::vector<FileInfo> children;

				if (arc_rec.loaded == false)
				{
					processing = false;
					return;
				}

				if (arc_rec.valid == false)
				{
					file_mutex.lock();
					file_children_map[map_key] = children;
					file_mutex.unlock();
					processing = false;
					return;
				}

				const VBFArchive::TreeNode* node = arc_rec.archive.find_node(folder_str);

				if (!node)
				{
					file_mutex.lock();
					file_children_map[map_key] = children;
					file_mutex.unlock();
					processing = false;
					return;
				}

				for (VBFArchive::TreeNode* child : node->children)
				{
					FileInfo info;

					info.name = child->name_segment;
					info.typeID = child->children.empty() ? FileInfo::File : FileInfo::Folder;

					if (info.typeID == FileInfo::File && filter.empty())
					{
						continue;
					}

					if (info.typeID != FileInfo::Folder)
					{
						if (const VBFArchive::File* file = arc_rec.archive.find_file(child->full_name()))
						{
							char buffer[128] = {};
							info.size = file->uncomp_size;
							info.size_str = StrFormatByteSize64A(info.size, buffer, sizeof(buffer));
						}
					}

					if (info.typeID == FileInfo::Folder)
					{
						info.type_str = "File Folder";
					}
					else
					{
						info.type_str = IO::GetExtension(info.name);
						if (info.type_str != "dae.phyre" && info.type_str != "dds.phyre" &&
							info.type_str != "win.mab" && info.type_str != "win.sab")
						{
							info.type_str = IO::GetLastExtension(info.type_str);
						}
						if (filter.size() && filter[0] != "*")
						{
							bool filter_passed = false;
							for (auto& filter : filter)
							{
								if (filter == info.type_str)
								{
									filter_passed = true;
									break;
								}
							}
							if (!filter_passed)
							{
								continue;
							}
						}
						if (!info.type_str.empty())
						{
							info.typeID = get_type_id(info.type_str);
							info.type_str += " ";
						}
						for (auto& c : info.type_str) c = toupper(c);
						info.type_str += "File";
					}

					children.push_back(info);
				}

				if (!sort_specs.empty())
				{
					std::sort(children.begin(), children.end(), [&sort_specs](const FileInfo& a, const FileInfo& b)
					{
						for (size_t n = 0; n < sort_specs.size(); n++)
						{
							int64_t delta = 0;
							switch (sort_specs[n].ColumnUserID)
							{
							case FileInfo::FieldID::Name:			delta = (strcmp(a.name.c_str(), b.name.c_str()));			break;
							case FileInfo::FieldID::Size:			delta = a.size - b.size;									break;
							case FileInfo::FieldID::DateModified:	delta = a.date_modified - b.date_modified;					break;
							case FileInfo::FieldID::DateCreated:	delta = a.date_created - b.date_created;					break;
							case FileInfo::FieldID::Type:			delta = (strcmp(a.type_str.c_str(), b.type_str.c_str()));	break;
							default: CONVASSERT(0); break;
							}
							if (delta < 0)
								return sort_specs[n].SortDirection == ImGuiSortDirection_Ascending;
							if (delta > 0)
								return sort_specs[n].SortDirection == ImGuiSortDirection_Descending;
						}
						return false;
					});
				}

				file_mutex.lock();
				file_children_map[map_key] = children;
				file_mutex.unlock();
				processing = false;
			});
		}

		if (is_new)
		{
			return {};
		}
		return it->second;
	}

	std::vector<FileInfo> get_child_files(const std::string& folder, const std::vector<std::string>& filter, bool& is_new, const std::vector<ImGuiTableColumnSortSpecs>& sort_specs)
	{
		decltype(file_children_map)::iterator it;
		file_mutex.lock_shared();
		it = file_children_map.find(folder);
		file_mutex.unlock_shared();
		is_new = it == file_children_map.end();

		if (bool& processing = file_processing_map.insert({ folder, false }).first->second; !processing)
		{
			processing = true;
			GUI::thread_pool.push([&processing, filter, folder, sort_specs](int)
			{
				std::vector<FileInfo> children;

				if (std::filesystem::exists(folder) && std::filesystem::is_directory(folder))
				{
					for (const auto& dir_entry : std::filesystem::directory_iterator(folder))
					{
						auto& entry_path = dir_entry.path();
						if (is_wide(entry_path) || GetFileAttributesA(entry_path.string().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT)
						{
							continue;
						}
						if (struct _stat64 stat_buffer;
							(_stat64(entry_path.string().c_str(), &stat_buffer) == 0 && (stat_buffer.st_mode & (S_IFDIR | S_IFREG)) != 0)
						) {
							try
							{
								WIN32_FIND_DATA search_data;
								SYSTEMTIME st;

								FileInfo info;
								char buffer[128] = {};
								info.name = entry_path.filename().string();
								info.typeID = stat_buffer.st_mode & S_IFDIR ? FileInfo::Folder : FileInfo::File;

								if (info.typeID == FileInfo::File && filter.empty())
								{
									continue;
								}

								if (FindFirstFileA(entry_path.string().c_str(), &search_data) != INVALID_HANDLE_VALUE)
								{
									info.size = (int64_t(search_data.nFileSizeHigh) << 32) | int64_t(search_data.nFileSizeLow);
									info.date_modified = (int64_t(search_data.ftLastWriteTime.dwHighDateTime) << 32) | int64_t(search_data.ftLastWriteTime.dwLowDateTime);
									info.date_created = (int64_t(search_data.ftCreationTime.dwHighDateTime) << 32) | int64_t(search_data.ftCreationTime.dwLowDateTime);

									if (info.typeID != FileInfo::Folder)
									{
										info.size_str = StrFormatByteSize64A(info.size, buffer, sizeof(buffer));
									}

									if (FileTimeToSystemTime(&search_data.ftLastWriteTime, &st))
									{
										if (GetDateFormatA(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, buffer, sizeof(buffer)) > 0)
										{
											info.date_modified_str = buffer;
										}
									}

									if (FileTimeToSystemTime(&search_data.ftCreationTime, &st))
									{
										if (GetDateFormatA(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, buffer, sizeof(buffer)) > 0)
										{
											info.date_created_str = buffer;
										}
									}
								}
								if (info.typeID == FileInfo::Folder)
								{
									info.type_str = "File Folder";
								}
								else
								{
									info.type_str = IO::GetExtension(info.name);
									if (info.type_str != "dae.phyre" && info.type_str != "dds.phyre" &&
										info.type_str != "win.mab" && info.type_str != "win.sab")
									{
										info.type_str = IO::GetLastExtension(info.type_str);
									}
									if (filter.size() && filter[0] != "*")
									{
										bool filter_passed = false;
										for (auto& filter : filter)
										{
											if (filter == info.type_str)
											{
												filter_passed = true;
												break;
											}
										}
										if (!filter_passed)
										{
											continue;
										}
									}
									if (!info.type_str.empty())
									{
										info.typeID = get_type_id(info.type_str);
										info.type_str += " ";
									}
									for (auto& c : info.type_str) c = toupper(c);
									info.type_str += "File";
								}
								children.push_back(info);
							}
							catch (const std::exception&)
							{
								continue;
							}
						}
					}
				}

				if (!sort_specs.empty())
				{
					std::sort(children.begin(), children.end(), [&sort_specs](const FileInfo& a, const FileInfo& b)
					{
						for (size_t n = 0; n < sort_specs.size(); n++)
						{
							int64_t delta = 0;
							switch (sort_specs[n].ColumnUserID)
							{
							case FileInfo::FieldID::Name:			delta = (strcmp(a.name.c_str(), b.name.c_str()));			break;
							case FileInfo::FieldID::Size:			delta = a.size - b.size;									break;
							case FileInfo::FieldID::DateModified:	delta = a.date_modified - b.date_modified;					break;
							case FileInfo::FieldID::DateCreated:	delta = a.date_created - b.date_created;					break;
							case FileInfo::FieldID::Type:			delta = (strcmp(a.type_str.c_str(), b.type_str.c_str()));	break;
							default: CONVASSERT(0); break;
							}
							if (delta < 0)
								return sort_specs[n].SortDirection == ImGuiSortDirection_Ascending;
							if (delta > 0)
								return sort_specs[n].SortDirection == ImGuiSortDirection_Descending;
						}
						return false;
					});
				}

				file_mutex.lock();
				file_children_map[folder] = children;
				file_mutex.unlock();
				processing = false;
			});
		}

		if (is_new)
		{
			return {};
		}
		return it->second;
	}

	std::vector<std::string> get_child_folders(const std::string& folder, bool& is_new)
	{
		decltype(folder_children_map)::iterator it;
		folder_mutex.lock_shared();
		it = folder_children_map.find(folder);
		folder_mutex.unlock_shared();
		is_new = it == folder_children_map.end();

		if (bool& processing = folder_processing_map.insert({ folder, false }).first->second; !processing)
		{
			processing = true;
			GUI::thread_pool.push([&processing, folder](int)
			{
				std::vector<std::string> children;

				if (std::filesystem::exists(folder) && std::filesystem::is_directory(folder))
				{
					for (const auto& dir_entry : std::filesystem::directory_iterator(folder))
					{
						auto& entry_path = dir_entry.path();
						if (is_wide(entry_path) || GetFileAttributesA(entry_path.string().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT)
						{
							continue;
						}
						//if (entry_folder.string().ends_with(".vbf")) __debugbreak();
						if (struct _stat64 stat_buffer;
							(_stat64(entry_path.string().c_str(), &stat_buffer) == 0 && ((stat_buffer.st_mode & S_IFDIR) != 0) ||
								((stat_buffer.st_mode & S_IFREG) != 0 && entry_path.string().ends_with(".vbf")))
							) {
							children.push_back(entry_path.filename().string());
						}
					}
				}
				folder_mutex.lock();
				folder_children_map[folder] = children;
				folder_mutex.unlock();
				processing = false;
			});
		}

		if (is_new)
		{
			return {};
		}
		return it->second;
	}

	void open_natively(std::string path)
	{
		std::replace(path.begin(), path.end(), '/', '\\');
		ShellExecuteA(NULL, "open", ("\"" + path + "\"").c_str(), NULL, NULL, SW_SHOWDEFAULT);
	}

	std::string right_aligned_elipsis_str(const std::vector<std::string>& folder, float max_len)
	{
		if (folder.empty())
		{
			return "";
		}
		std::string str = IO::Join(folder);
		if (ImGui::CalcTextSize(str.c_str()).x <= max_len)
		{
			return str;
		}
		float elipsis_len = ImGui::CalcTextSize("...").x;
		max_len -= elipsis_len;
		for (auto it = folder.begin() + 1; it != folder.end(); ++it)
		{
			str = IO::Join(std::vector<std::string>(it, folder.end()));
			if (ImGui::CalcTextSize(str.c_str()).x <= max_len)
			{
				return "..." + str;
			}
		}
		return "...";
	}
}

FileBrowser::FileBrowser(GUI& gui) :
	gui(gui)
{}

bool FileBrowser::init(HWND window)
{
	free();

	this->window = window;

	if (OleInitialize(NULL) != S_OK)
	{
		std::cerr << "Failed to initialise COM library" << std::endl;
		return false;
	}

	drop_target = new ClipDropTarget(*this);
	drop_target->vbf_helper = new VBFInjectClipHelper(*this);
	RegisterDragDrop(window, drop_target);

	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	DWORD drive_mask = GetLogicalDrives();
	char drive_path[4] = "@:\\";
	for (int i = 0; i < 26; ++i)
	{
		drive_path[0]++;
		if (!(drive_mask & (1 << i)))
		{
			continue;
		}
		;
		UINT type = GetDriveTypeA(drive_path);
		if (type == DRIVE_REMOVABLE || type == DRIVE_FIXED || type == DRIVE_REMOTE)
		{
			drives.push_back(std::string(drive_path, drive_path + 2));
		}
	}

	load_pinned_folders();

	folder_history_end = folder_history.end();

	file_icon = GUIIcon(file_icon_small_data, sizeof(file_icon_small_data));
	folder_icon = GUIIcon(folder_icon_small_data, sizeof(folder_icon_small_data));
	archive_icon = GUIIcon(archive_icon_small_data, sizeof(archive_icon_small_data));
	model_icon = GUIIcon(model_icon_small_data, sizeof(model_icon_small_data));
	image_icon = GUIIcon(image_icon_small_data, sizeof(image_icon_small_data));
	text_icon = GUIIcon(text_icon_small_data, sizeof(text_icon_small_data));
	music_icon = GUIIcon(music_icon_small_data, sizeof(music_icon_small_data));
	video_icon = GUIIcon(video_icon_small_data, sizeof(video_icon_small_data));
	exe_icon = GUIIcon(exe_icon_small_data, sizeof(exe_icon_small_data));
	dll_icon = GUIIcon(dll_icon_small_data, sizeof(dll_icon_small_data));
	left_icon = GUIIcon(left_icon_data, sizeof(left_icon_data));
	right_icon = GUIIcon(right_icon_data, sizeof(right_icon_data));
	up_icon = GUIIcon(up_icon_data, sizeof(up_icon_data));
	down_icon = GUIIcon(down_icon_data, sizeof(down_icon_data));
	left_icon_tiny = GUIIcon(left_icon_tiny_data, sizeof(left_icon_tiny_data));
	right_icon_tiny = GUIIcon(right_icon_tiny_data, sizeof(right_icon_tiny_data));
	pin_icon = GUIIcon(pin_icon_small_data, sizeof(pin_icon_small_data));
	return true;
}

void FileBrowser::free()
{
	GUI::thread_pool.stop();
	GUI::thread_pool.~thread_pool(); //library is buggy and doesn't allow pool reuse
	new(&GUI::thread_pool) ctpl::thread_pool(GUI::thread_count_max);

	folder_history.clear();

	drives.clear();
	folder_icon.free();
	file_icon.free();
	archive_icon.free();
	model_icon.free();
	image_icon.free();
	text_icon.free();
	music_icon.free();
	exe_icon.free();
	dll_icon.free();
	left_icon.free();
	right_icon.free();
	up_icon.free();
	down_icon.free();
	left_icon_tiny.free();
	right_icon_tiny.free();
	pin_icon.free();

	RevokeDragDrop(window);
	OleUninitialize();

	if (drop_target)
	{
		delete drop_target->vbf_helper;
		delete drop_target;
	}

	window = nullptr;
}

bool FileBrowser::set_current_path(const std::string& path)
{
	try
	{
		size_t last_parent = path.find_last_of("/\\");
		std::string file_name = last_parent == std::string::npos ? path : path.substr(last_parent + 1);
		std::string parent_path = last_parent == std::string::npos ? "" : path.substr(0, last_parent);
		size_t vbf_pos = path.find(".vbf");
		if (vbf_pos == std::string::npos)
		{
			parent_path = std::filesystem::absolute(parent_path).string();
		}
		else
		{
			vbf_pos += 4;
			parent_path = std::filesystem::absolute(parent_path.substr(0, vbf_pos)).string() + parent_path.substr(vbf_pos);
		}
		set_current_folder(IO::Segment(parent_path));
		cur_filename = file_name;
	}
	catch (const std::exception&) 
	{
		return false;
	}
	return true;
}

FileBrowser::DialogCode FileBrowser::run_dialog()
{
	draw_content();

	DialogCode returned_code = dialog_code;
	dialog_code = DialogCode_None;
	return returned_code;
}

void FileBrowser::draw_popup_messages()
{
	for (auto it = popup_messages.begin(); it != popup_messages.end();)
	{
		if (it->first < 0.0f)
		{
			it = popup_messages.erase(it);
			continue;
		}
		it->first -= ImGui::GetIO().DeltaTime;
		float alpha = std::min(0.5f, it->first) * 2.0f;
		it->second(alpha);
		++it;
	}
}

void FileBrowser::popup_error(const std::string& msg)
{
	ImVec2 pos = ImGui::GetCursorScreenPos();

	popup_messages.push_back(std::make_pair<float, std::function<void(float)>>(2.0f, [pos, msg](float alpha)
	{
		ImVec4 text_col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.25f, 0.25f, alpha));
		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.1f, 0.025f, 0.025f, alpha));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, text_col.y * 0.8f, text_col.z * 0.8f, alpha));
		ImGui::SetNextWindowPos(pos);
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(msg.c_str());
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
		ImGui::PopStyleColor(3);
	}));
}

void FileBrowser::popup_info(const std::string& msg)
{
	ImVec2 pos = ImGui::GetCursorScreenPos();

	popup_messages.push_back(std::make_pair<float, std::function<void(float)>>(2.0f, [pos, msg](float alpha)
	{
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.25f, 0.25f, alpha));
		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.1f, 0.025f, 0.025f, alpha));
		ImGui::SetNextWindowPos(pos);
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(msg.c_str());
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
		ImGui::PopStyleColor(2);
	}));
}

bool FileBrowser::valid(const std::string& path)
{
	std::string arc_path = path, file_path;
	if (path.find(".vbf") != std::string::npos)
	{
		if (!VBFUtils::Separate(path, arc_path, file_path))
		{
			return false;
		}
		if (std::filesystem::exists(arc_path) == false)
		{
			return false;
		}
		if (file_path.size())
		{
			auto find = archives.find(arc_path);
			if (find == archives.end())
			{
				return true;
			}
			if (!find->second.loaded)
			{
				return true;
			}
			if (!find->second.valid)
			{
				return false;
			}
			if (const VBFArchive::TreeNode* node = find->second.archive.find_node(file_path); !node)
			{
				return false;
			}
		}

		return true;
	}

	if (std::filesystem::exists(path) == false)
	{
		return false;
	}
	
	return true;
}

bool FileBrowser::valid(const std::vector<std::string>& path)
{
	return valid(IO::Join(path));
}

void FileBrowser::draw_content()
{
	if (!valid(IO::Join(cur_parent_folder)))
	{
		set_current_folder({ "C:" });
	}

	filter_index = std::min(filter_index, filters.size() - 1);

	ImGui::BeginChild("GUIFileBrowser", ImVec2(0, 0), true, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking);

	draw_archive_modal_window();
	draw_rename_modal_window();
	draw_nav_bar();

	ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;
	ImGuiID dockspaceID = ImGui::GetID("GUIFileBrowser_dockspace");
	if (!ImGui::DockBuilderGetNode(dockspaceID))
	{
		ImGui::DockBuilderRemoveNode(dockspaceID);
		ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_None);

		ImGuiID dock_main_id = dockspaceID;
		ImGuiID dock_down_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.2f, nullptr, &dock_main_id);
		ImGuiID dock_left_id, dock_right_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.5f, nullptr, &dock_left_id);
		ImGuiID dock_left_top_id, dock_left_bottom_id = ImGui::DockBuilderSplitNode(dock_left_id, ImGuiDir_Down, 0.2f, nullptr, &dock_left_top_id);

		ImGui::DockBuilderDockWindow("current_folder_pane", dock_right_id);
		ImGui::DockBuilderDockWindow("file_tree_pane", dock_left_top_id);
		ImGui::DockBuilderDockWindow("quick_access_pane", dock_left_bottom_id);
		ImGui::DockBuilderDockWindow("footer", dock_down_id);

		ImGui::DockBuilderGetNode(dock_down_id)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
		ImGui::DockBuilderGetNode(dock_left_top_id)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
		ImGui::DockBuilderGetNode(dock_left_bottom_id)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
		ImGui::DockBuilderGetNode(dock_right_id)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

		ImGui::DockBuilderFinish(dock_main_id);
	}
	ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), dockspaceFlags);

	draw_folder_tree_pane();
	draw_current_folder_pane();
	draw_quick_access_pane();
	draw_footer();

	draw_popup_messages();

	ImGui::EndChild();

	if (dragging_current_file)
	{
		drag_current_file();
	}
}

void FileBrowser::draw_rename_modal_window()
{
	static bool modal_open = false;
	static char buffer[128]{};

	if (cur_filename.empty())
	{
		return;
	}

	if (renaming_current_file && modal_open == false)
	{
		ImGui::OpenPopup("Rename File/Folder");
		strcpy_s(buffer, sizeof(buffer), cur_filename.c_str());
		modal_open = true;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15.f, 20.f));
	if (ImGui::BeginPopupModal("Rename File/Folder", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (ImGui::InputText("#NewNameInput", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			renaming_current_file = false;
			rename_current_file(buffer);
		}
		float button_width = 100.0f;
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - (2.0f * button_width + ImGui::GetStyle().ItemSpacing.x)) / 2.0f);

		if (ImGui::Button("Rename", ImVec2(button_width, 0.0f)))
		{
			renaming_current_file = false;				
			rename_current_file(buffer);
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(button_width, 0.0f)))
		{
			renaming_current_file = false;
		}

		if (renaming_current_file == false)
		{
			ImGui::CloseCurrentPopup();
			modal_open = false;
		}

		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();
}

FileBrowser::ArchiveRecord& FileBrowser::get_vbf(const std::string& path)
{
	auto result = archives.insert({ path, ArchiveRecord() });
	ArchiveRecord& arc_rec = result.first->second;

	if (result.second) //new insertion
	{
		GUI::thread_pool.push([&arc_rec, path](int)
		{
			arc_rec.valid = arc_rec.archive.load(path);
			arc_rec.loaded = true;
		});
	}
	return arc_rec;
}

void FileBrowser::drag_current_file()
{
	ClipDragDrop::RunDragDrop(*this, dragging_left);

	POINT cursor_pos;
	GetCursorPos(&cursor_pos);
	if (dragging_left)
		PostMessage(window, WM_LBUTTONUP, MK_LBUTTON, MAKELPARAM(cursor_pos.x, cursor_pos.y));
	else PostMessage(window, WM_RBUTTONUP, MK_RBUTTON, MAKELPARAM(cursor_pos.x, cursor_pos.y));

	dragging_current_file = false;
}

void FileBrowser::create_new_folder()
{
	std::string cur_folder_str = IO::Join(cur_parent_folder);

	if (!std::filesystem::exists(cur_folder_str) || !std::filesystem::is_directory(cur_folder_str))
	{
		gbl_err << "Directory does not exist to create new folder in" << std::endl;
		return;
	}

	std::string new_folder_name = "New Folder";
	std::string new_folder_path = cur_folder_str + "/" + new_folder_name;
	
	for (int i = 1; std::filesystem::exists(new_folder_path); ++i)
	{
		new_folder_name = "New Folder (" + std::to_string(i) + ")";
		new_folder_path = cur_folder_str + "/" + new_folder_name;
	}
	new_folder_path = std::filesystem::weakly_canonical(new_folder_path).string();
	new_folder_path.push_back('\0'), new_folder_path.push_back('\0');

	int result = SHCreateDirectoryExA(NULL, new_folder_path.c_str(), NULL);
	if (result != 0)
	{
		gbl_err << "Failed to create new folder in current directory, error_code " << result << std::endl;
		return;
	}

	cur_filename = new_folder_name;
	focus_current_folder = true;
}

void FileBrowser::rename_current_file(const std::string& new_name)
{
	if (cur_filename.empty())
	{
		gbl_err << "Failed to rename file: current file name is empty" << std::endl;
		return;
	}
	std::string old_path = IO::Join(cur_parent_folder) + "/" + cur_filename;

	if (!std::filesystem::exists(old_path))
	{
		gbl_err << "Path does not exist to rename" << std::endl;
		return;
	}
	std::string new_path = IO::Join(cur_parent_folder) + "/" + new_name;
	old_path = std::filesystem::weakly_canonical(old_path).string();
	new_path = std::filesystem::weakly_canonical(new_path).string();
	old_path.push_back('\0'), old_path.push_back('\0');
	new_path.push_back('\0'), new_path.push_back('\0');

	SHFILEOPSTRUCTA fileOp = { 0 };
	fileOp.wFunc = FO_RENAME;
	fileOp.pFrom = old_path.data();
	fileOp.pTo = new_path.data(); 
	fileOp.fFlags = FOF_ALLOWUNDO;

	int result = SHFileOperation(&fileOp);
	if (result != 0)
	{
		gbl_err << "Failed to rename current file/folder, error_code " << result << std::endl;
		return;
	}
	cur_filename = new_path;
	focus_current_folder = true;
}

void FileBrowser::trash_current_file()
{
	if (cur_filename.empty())
	{
		gbl_err << "Failed to trash file: current file name is empty" << std::endl;
		return;
	}
	std::string path = IO::Join(cur_parent_folder) + "/" + cur_filename;

	if (!std::filesystem::exists(path))
	{
		gbl_err << "Path does not exist to trash" << std::endl;
		return;
	}
	path = std::filesystem::weakly_canonical(path).string();
	path.push_back('\0'), path.push_back('\0');

	SHFILEOPSTRUCTA fileOp = { 0 };
	fileOp.wFunc = FO_DELETE;
	fileOp.pFrom = path.data();
	fileOp.fFlags = FOF_ALLOWUNDO;

	int result = SHFileOperation(&fileOp);
	if (result != 0)
	{
		gbl_err << "Failed to trash current file/folder, error_code " << result << std::endl;
		return;
	}
	cur_filename = "";
	focus_current_folder = true;
}

void FileBrowser::draw_archive_modal_window()
{
	static bool modal_open = false;
	bool show_modal = false;
	for (auto it = archives.begin(); it != archives.end(); ++it)
	{
		ArchiveRecord& arc_rec = it->second;
		if (arc_rec.loaded == false)
		{
			show_modal = true;
		}
	}
	if (show_modal && modal_open == false)
	{
		ImGui::OpenPopup("Loading VBF Archive(s)");
		modal_open = true;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15.f, 20.f));
	if (ImGui::BeginPopupModal("Loading VBF Archive(s)", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		for (auto it = archives.begin(); it != archives.end(); ++it)
		{
			ArchiveRecord& arc_rec = it->second;
			if (arc_rec.loaded == false)
			{
				ImGui::Text(("Loading " + it->first).c_str());
			}
		}
		if (show_modal == false)
		{
			ImGui::CloseCurrentPopup();
			modal_open = false;
			focus_current_folder = true;
		}
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();
}

void FileBrowser::draw_current_folder_pane()
{
	ImGui::Begin("current_folder_pane", nullptr, 0);

	std::string cur_folder_str = IO::Join(cur_parent_folder);

	static ImGuiTableFlags flags =
		ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
		ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_NoBordersInBodyUntilResize |
		ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Sortable;

	bool in_vbf = cur_folder_str.find(".vbf") != std::string::npos;

	if (ImGui::BeginTable("file_table", in_vbf ? 3 : 5, flags, ImVec2(0.0f, 0.0f), 0.0f))
	{
		ImGui::TableSetupColumn("Name", 0, 200.0f, FileInfo::FieldID::Name);
		ImGui::TableSetupColumn("Type", 0, 100.0f, FileInfo::FieldID::Type);
		ImGui::TableSetupColumn("Size", 0, 100.0f, FileInfo::FieldID::Size);
		if (!in_vbf)
		{
			ImGui::TableSetupColumn("Date Modified", 0, 0.0f, FileInfo::FieldID::DateModified);
			ImGui::TableSetupColumn("Date Created", 0, 0.0f, FileInfo::FieldID::DateCreated);
		}
		ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
		ImGui::TableHeadersRow();

		ImGuiTableSortSpecs* imgui_sort_specs = ImGui::TableGetSortSpecs();
		std::vector<ImGuiTableColumnSortSpecs> sort_specs(imgui_sort_specs ? imgui_sort_specs->SpecsCount : 0);

		for (size_t i = 0; i < sort_specs.size(); ++i)
		{
			sort_specs[i] = imgui_sort_specs->Specs[i];
		}

		bool is_new = false;
		std::vector<FileInfo> children;
		std::vector<std::string> filter = filter_index < filters.size() ? filters[filter_index].second : std::vector<std::string>();

		if (in_vbf)
		{
			std::string arc_path, file_name;
			if (VBFUtils::Separate(cur_folder_str, arc_path, file_name))
			{
				auto find = archives.find(arc_path);
				if (find != archives.end())
				{
					children = get_child_files(file_name, filter, is_new, sort_specs, find->second);
				}
			}
		}
		else
		{
			children = get_child_files(cur_folder_str, filter, is_new, sort_specs);
		}

		if (is_new)
		{
			ImGui::EndTable();

			std::string text = "loading...";
			ImGui::Spacing();
			ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(text.c_str()).x) * 0.5f);
			ImGui::Text(text.c_str());
		}
		else if (children.empty())
		{
			ImGui::EndTable();

			std::string text = "This folder is empty.";
			ImGui::Spacing();
			ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(text.c_str()).x) * 0.5f);
			ImGui::Text(text.c_str());
		}
		else
		{
			for (int i = 0; i < children.size(); ++i)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				GUIIcon* icon = nullptr;
				ImVec4 icon_tint;
				switch (children[i].typeID)
				{
					case FileInfo::Folder :		icon = &folder_icon;	icon_tint = ImVec4(1.0f, 0.92f, 0.36f, 1.0f); break;
					case FileInfo::Archive :	icon = &archive_icon;	icon_tint = ImVec4(1.0f, 0.9f, 0.16f, 1.0f) ; break;
					case FileInfo::VBFArchive :	icon = &archive_icon;	icon_tint = ImVec4(0.61f, 1.0f, 0.46f, 1.0f); break;
					case FileInfo::Image :		icon = &image_icon; 	icon_tint = ImVec4(0.55f, 1.0f, 0.88f, 1.0f); break;
					case FileInfo::Model :		icon = &model_icon; 	icon_tint = ImVec4(0.55f, 1.0f, 0.88f, 1.0f); break;
					case FileInfo::Music :		icon = &music_icon; 	icon_tint = ImVec4(0.55f, 1.0f, 0.88f, 1.0f); break;
					case FileInfo::Text :		icon = &text_icon; 		icon_tint = ImVec4(0.79f, 0.91f, 1.0f, 1.0f); break;
					case FileInfo::Video :		icon = &video_icon;		icon_tint = ImVec4(0.55f, 1.0f, 0.88f, 1.0f); break;
					case FileInfo::Exe :		icon = &exe_icon; 		icon_tint = ImVec4(0.66f, 0.54f, 1.0f, 1.0f); break;
					case FileInfo::Dll :		icon = &dll_icon; 		icon_tint = ImVec4(0.79f, 0.72f, 1.0f, 1.0f); break;
					default:					icon = &file_icon;		icon_tint = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
				}

				ImGui::Image(*icon, ImVec2((float)icon->width, (float)icon->height), ImVec2(0, 0), ImVec2(1, 1), icon_tint);
				ImGui::SameLine();

				if (ImGui::Selectable(children[i].name.c_str(), compare(children[i].name, cur_filename), ImGuiSelectableFlags_SpanAllColumns)
					|| ImGui::IsItemFocused())
				{
					cur_filename = children[i].name;
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) ||
					ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter))
				) {
					if (children[i].typeID & (FileInfo::Folder | FileInfo::VBFArchive))
					{
						std::vector<std::string> new_folder = cur_parent_folder;
						new_folder.push_back(children[i].name);
						set_current_folder(new_folder);
					}
					else
					{ 
						if (!confirm_overwrite || gui.confirm("Overwrite existing file(s)?"))
						{
							dialog_code = DialogCode_Confirm;
						}
					}			
				}

				if (ImGui::IsItemHovered() && (ImGui::IsMouseDragging(ImGuiMouseButton_Left) || ImGui::IsMouseDragging(ImGuiMouseButton_Right)))
				{
					dragging_current_file = true;
					dragging_left = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
				}

				if ((!in_vbf || children[i].typeID & FileInfo::Folder) && ImGui::BeginPopupContextItem(("cmenu_" + IO::Join(cur_parent_folder) + children[i].name).c_str()))
				{
					if (children[i].typeID & (FileInfo::Folder | FileInfo::VBFArchive))
					{
						std::vector<std::string> new_folder = cur_parent_folder;
						new_folder.push_back(children[i].name);
						if (ImGui::Selectable("Open")) set_current_folder(new_folder);
						if (ImGui::Selectable("Pin")) push_pinned_folder(new_folder);
						if (!in_vbf && children[i].typeID & FileInfo::Folder)
						{
							ImGui::Separator();
							if (ImGui::Selectable("Open Folder in Explorer")) open_natively(IO::Join(new_folder));
						}
					}
					else if(!in_vbf)
					{
						ImGui::Separator();
						if (ImGui::Selectable("Open Location in Explorer")) open_natively(IO::Join(cur_parent_folder));
					}
					if (!in_vbf)
					{
						ImGui::Separator();
						if (ImGui::Selectable("Rename"))
						{
							cur_filename = children[i].name;
							renaming_current_file = true;
						}
						else if (ImGui::Selectable("Trash"))
						{
							cur_filename = children[i].name;
							trash_current_file();
						}
					}

					ImGui::EndPopup();
				}

				ImGui::TableNextColumn();
				ImGui::Text(children[i].type_str.c_str());
				ImGui::TableNextColumn();
				ImGui::Text(children[i].size_str.c_str());
				if (!in_vbf)
				{
					ImGui::TableNextColumn();
					ImGui::Text(children[i].date_modified_str.c_str());
					ImGui::TableNextColumn();
					ImGui::Text(children[i].date_created_str.c_str());
				}
			}
			ImGui::EndTable();
		}
	}

	ImGui::InvisibleButton("parent_cmenu_dummy_button", ImVec2(ImGui::GetContentRegionAvail().x, std::max(100.0f, ImGui::GetContentRegionAvail().y)));

	if (!in_vbf && ImGui::BeginPopupContextItem(("parent_cmenu_" + IO::Join(cur_parent_folder)).c_str()))
	{
		if (ImGui::Selectable("Open in Explorer"))
		{
			open_natively(IO::Join(cur_parent_folder));
		}
		ImGui::Separator();
		if (ImGui::Selectable("New Folder"))
		{
			create_new_folder();
		}
		ImGui::EndPopup();
	}

	ImGui::End();
}

void FileBrowser::draw_folder_tree_pane()
{
	ImGui::Begin("file_tree_pane", nullptr, 0);

	for (auto& drive : drives)
	{
		draw_tree({ drive });
	}
	focus_current_folder = false;

	ImGui::End();
}

ImRect FileBrowser::draw_tree(const std::vector<std::string>& path)
{
	bool in_cur_path = false, is_cur_path = false, clicked = false;
	compare(path, cur_parent_folder, in_cur_path, is_cur_path);

	std::string path_str = IO::Join(path);

	bool is_new = false;
	std::vector<std::string> children = get_child_folders(path_str, is_new);
	if (in_cur_path && focus_current_folder)
	{
		while (is_new)
		{
			children = get_child_folders(path_str, is_new);
		}
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);
	}

	ImRect nodeRect = draw_folder_tree_node(path, clicked, is_cur_path, children.empty(), 
	[this, &path_str, &path]() 
	{	
		if (ImGui::BeginPopupContextItem(path_str.c_str()))
		{
			if (ImGui::Selectable("Open")) set_current_folder(path);
			if (ImGui::Selectable("Pin")) push_pinned_folder(path);
			if (ImGui::Selectable("Open Folder in Explorer")) open_natively(path_str);

			ImGui::EndPopup();
		}
	},			
	generator::create(children),
	[this, &path_str, &path](void* child_ptr)
	{
		std::string& child = *(std::string*)child_ptr;
		std::vector<std::string> child_path = path;
		child_path.push_back(child);
		if (child.ends_with(".vbf"))
		{
			return draw_vbf_tree(child_path, path_str + "/" + child, child);
		}
		else
		{
			return draw_tree(child_path);
		}
	});

	if (clicked)
	{
		set_current_folder(path);
	}

	return nodeRect;
}

ImRect FileBrowser::draw_folder_tree_node(const std::vector<std::string>& path, bool& clicked, bool selected, bool leaf, const std::function<void()> pre_children, generator child_generator, const std::function<ImRect(void*)>& child_operator, int type_code)
{
	switch (type_code)
	{
	case 1: ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.65f, 0.65f, 1.0f)); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.45f, 0.45f, 1.0f)); break;
	case 2: ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 1.0f, 0.65f, 1.0f)); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.8f, 0.45f, 1.0f)); break;
	case 3: ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 1.0f, 0.8f, 1.0f)); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 0.6f, 1.0f)); break;
	default: ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.65f, 1.0f));
	}
	bool open = ImGui::TreeNodeEx(("##" + IO::Join(path)).c_str(), ImGuiTreeNodeFlags_SpanAvailWidth | (leaf ? ImGuiTreeNodeFlags_Leaf : 0));
	ImGui::PopStyleColor();

	ImVec4 text_colour = ImGui::GetStyleColorVec4(ImGuiCol_Text);
	ImVec4 selected_colour = ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive);
	ImColor tree_line_colour = ImColor(text_colour.x * 0.25f, text_colour.y * 0.25f, text_colour.z * 0.25f, 1.0f);
	ImColor tree_selected_colour = ImColor(selected_colour.x, selected_colour.y, selected_colour.z, selected_colour.w * 0.2f);
	ImVec2 verticalLineStart = ImGui::GetCursorScreenPos();
	ImRect nodeRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

	clicked = ImGui::IsItemClicked() || ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter));

	pre_children();

	if (selected)
	{
		ImGui::GetWindowDrawList()->AddRectFilled(nodeRect.Min, nodeRect.Max, tree_selected_colour);
		if (focus_current_folder)
		{
			ImGui::SetScrollHereY();
		}
	}
	ImGui::GetWindowDrawList()->AddText(ImVec2(nodeRect.Min.x + 18.0f, nodeRect.Min.y), ImColor(text_colour), path.back().c_str());

	if (type_code) ImGui::PopStyleColor();

	if (open)
	{
		const float SmallOffsetX = 11.0f; //for now, a hardcoded value; should take into account tree indent size

		verticalLineStart.x -= SmallOffsetX; //to nicely line up with the arrow symbol
		ImVec2 verticalLineEnd = verticalLineStart;

		while (void* child = child_generator.next())
		{
			const float HorizontalTreeLineSize = 8.0f; //chosen arbitrarily

			ImRect childRect = child_operator(child);

			const float midpoint = (childRect.Min.y + childRect.Max.y) / 2.0f;
			ImGui::GetWindowDrawList()->AddLine(ImVec2(verticalLineStart.x, midpoint), ImVec2(verticalLineStart.x + HorizontalTreeLineSize, midpoint), tree_line_colour);
			verticalLineEnd.y = midpoint;
		}

		ImGui::GetWindowDrawList()->AddLine(verticalLineStart, verticalLineEnd, tree_line_colour);

		ImGui::TreePop();
	}

	return nodeRect;
}

ImRect FileBrowser::draw_vbf_tree(const std::vector<std::string>& vbf_path, const std::string& vbf_path_str, const std::string& vbf_filename)
{
	ArchiveRecord& arc_rec = get_vbf(vbf_path_str);

	if (!arc_rec.loaded)
	{
		ImGui::Text((vbf_filename + " (loading...)").c_str());
		return ImRect(ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos());
	}

	std::vector<std::string> cur_arc_name;
	bool in_cur_path = false, is_cur_path = false, clicked = false;
	compare(vbf_path, cur_parent_folder, in_cur_path, is_cur_path);

	if (in_cur_path && focus_current_folder)
	{
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);
	}
	if (in_cur_path)
	{
		auto it = std::find_if(cur_parent_folder.begin(), cur_parent_folder.end(), [](const std::string& s) { return s.ends_with(".vbf"); });
		if (it != cur_parent_folder.end())
		{
			cur_arc_name.assign(it + 1, cur_parent_folder.end());
		}
	}

	ImRect nodeRect = draw_folder_tree_node(vbf_path, clicked, is_cur_path, !arc_rec.valid, 
	[this, &vbf_path_str, &vbf_path]()
	{
		if (ImGui::BeginPopupContextItem(vbf_path_str.c_str()))
		{
			if (ImGui::Selectable("Open")) push_pinned_folder(vbf_path);
			if (ImGui::Selectable("Pin")) push_pinned_folder(vbf_path);
			if (ImGui::Selectable("Open Location in Explorer")) open_natively(vbf_path_str);

			ImGui::EndPopup();
		}
	},
	!arc_rec.archive.name_tree.empty() ? generator::create(arc_rec.archive.name_tree[0].children) : generator(),
	[this, &vbf_path, &cur_arc_name](void* child_ptr)
	{
		VBFArchive::TreeNode& child = **(VBFArchive::TreeNode**)child_ptr;
		return draw_vbf_tree(vbf_path, cur_arc_name, { child.name_segment }, child);
	}, arc_rec.valid ? 2 : 1);

	if (clicked)
	{
		set_current_folder(vbf_path);
	}

	return nodeRect;
}

ImRect FileBrowser::draw_vbf_tree(const std::vector<std::string>& vbf_path, const std::vector<std::string>& cur_arc_name, const std::vector<std::string>& arc_foldername, const VBFArchive::TreeNode& node)
{
	bool in_cur_path = false, is_cur_path = false, clicked = false, leaf = true;
	compare(arc_foldername, cur_arc_name, in_cur_path, is_cur_path);

	std::vector<VBFArchive::TreeNode*> drawable_children;
	drawable_children.reserve(node.children.size());
	for (VBFArchive::TreeNode* child : node.children)
	{
		if (!child->children.empty())
		{
			drawable_children.push_back(child);
		}
	}

	if (in_cur_path && focus_current_folder)
	{
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);
	}

	ImRect nodeRect = draw_folder_tree_node(arc_foldername, clicked, is_cur_path, drawable_children.empty(), 
	[this, &vbf_path, &arc_foldername]()
	{
		if (ImGui::BeginPopupContextItem((IO::Join(vbf_path) + IO::Join(arc_foldername)).c_str()))
		{
			std::vector<std::string> folder;
			folder.reserve(vbf_path.size() + arc_foldername.size());
			folder.insert(folder.end(), vbf_path.begin(), vbf_path.end());
			folder.insert(folder.end(), arc_foldername.begin(), arc_foldername.end());
			set_current_folder(folder);

			if (ImGui::Selectable("Open")) push_pinned_folder(folder);
			if (ImGui::Selectable("Pin")) push_pinned_folder(folder);

			ImGui::EndPopup();
		}
	},
	generator::create(drawable_children),
	[this, &vbf_path, &arc_foldername, &cur_arc_name](void* child_ptr)
	{
		VBFArchive::TreeNode& child = **(VBFArchive::TreeNode**)child_ptr;
		std::vector<std::string> child_path = arc_foldername;
		child_path.push_back(child.name_segment);
		return draw_vbf_tree(vbf_path, cur_arc_name, child_path, child);
	}, 3);

	if (clicked)
	{
		std::vector<std::string> folder;
		folder.reserve(vbf_path.size() + arc_foldername.size() + 1);
		folder.insert(folder.end(), vbf_path.begin(), vbf_path.end());
		folder.insert(folder.end(), arc_foldername.begin(), arc_foldername.end());
		set_current_folder(folder);
	}

	return nodeRect;
}

void FileBrowser::draw_quick_access_pane()
{
	ImGui::Begin("quick_access_pane", nullptr, 0);

	if (pinned_folders.empty())
	{
		std::string text = "Pin folders to this pane";
		ImGui::Spacing();
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(text.c_str()).x) * 0.5f);
		ImGui::Text(text.c_str());
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 2.0f));
		for (size_t i = 0; i < pinned_folders.size(); ++i)
		{
			ImGui::PushID((int)i);
			if (ImGui::ImageButton(pin_icon, ImVec2((float)pin_icon.width, (float)pin_icon.height)))
			{
				ImGui::PopID();
				erase_pinned_folder(i--);
				continue;
			}
			ImGui::PopID();
			ImGui::SameLine();
			if (ImGui::Button((right_aligned_elipsis_str(IO::Segment(pinned_folders[i]), ImGui::GetContentRegionAvail().x) + "##pinned_" + std::to_string(i)).c_str()))
			{
				if (valid(pinned_folders[i]))
				{
					set_current_folder(IO::Segment(pinned_folders[i]));
				}
				else
				{
					popup_error("Folder path is invalid");
				}
			}
		}
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
	}

	ImGui::End();
}

void FileBrowser::draw_nav_bar()
{
	static char buffer[256] = {};

	if (ImGui::BeginMenuBar())
	{
		ImVec2 button_size = ImVec2(14.0f, 14.0f);
		float item_spacing = ImGui::GetStyle().ItemSpacing.x;
		float item_padding = ImGui::GetStyle().ItemInnerSpacing.x;

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

		if (ImGui::ImageButton(left_icon, button_size))
		{
			set_current_folder_to_prev();
		}
		//ImGui::Separator();
		if (ImGui::ImageButton(right_icon, button_size))
		{
			set_current_folder_to_next();
		}
		//ImGui::Separator();
		if (ImGui::ImageButton(up_icon, button_size))
		{
			if (!cur_parent_folder.empty())
			{
				set_current_folder(std::vector<std::string>(cur_parent_folder.begin(), cur_parent_folder.end() - 1));
			}
		}
		ImGui::Separator();

		static bool text_edit_mode = false;

		if (text_edit_mode)
		{
			static char text_buffer[1024];
			static bool text_edit_mode_open = false;
			if (text_edit_mode_open == false)
			{
				std::string cur_parent_folder_str = IO::Join(cur_parent_folder);
				strncpy_s(text_buffer, cur_parent_folder_str.c_str(), std::min(sizeof(text_buffer), strlen(cur_parent_folder_str.c_str()) + 1));
				text_edit_mode_open = true;
			}
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			if (ImGui::InputText("##cur_parent_folder_text_edit", text_buffer, sizeof(text_buffer), ImGuiInputTextFlags_EnterReturnsTrue) ||
				ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax())
			) {
				std::string cur_parent_path_str = IO::Normalise(text_buffer);

				if (valid(cur_parent_path_str))
				{
					set_current_path(cur_parent_path_str + "/");
				}
				else
				{
					ImGui::SetCursorPos(ImVec2((ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) / 2.0f, ImGui::GetItemRectMax().y));
					popup_error("Folder path was invalid");
				}
				text_edit_mode = false;
				text_edit_mode_open = false;
			}
		}
		else
		{
			ImVec2 address_start = ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMin().y);
			ImRect address_rect = ImRect(address_start, ImGui::GetWindowContentRegionMax());

			ImRect address_unused_rect = address_rect;
			address_unused_rect.Min.x += item_spacing;
			float unused_space_min = std::min(ImGui::GetWindowContentRegionMax().y / 3.0f, 300.0f);

			int64_t min_seg = int64_t(cur_parent_folder.size()) - 1;
			for (; min_seg >= 0; --min_seg)
			{
				float next_seg_space = item_spacing + item_padding * 2.0f + ImGui::CalcTextSize(cur_parent_folder[min_seg].c_str()).x + ((min_seg < int64_t(cur_parent_folder.size() - 1)) ? item_spacing + right_icon_tiny.width : 0.0f);

				if (min_seg < (int64_t)cur_parent_folder.size() - 1 && address_unused_rect.Min.x + next_seg_space >= address_unused_rect.Max.x - unused_space_min)
				{
					break;
				}
				address_unused_rect.Min.x += next_seg_space;
			}
			min_seg = min_seg + 1;

			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(address_unused_rect.Min, address_unused_rect.Max))
			{
				text_edit_mode = true;
			}

			bool seg_button_clicked = false;

			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + item_spacing);
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 20.f));
			for (size_t i = (size_t)min_seg; i < cur_parent_folder.size(); ++i)
			{
				ImGui::SameLine();
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + item_spacing);
				ImGui::Button((cur_parent_folder[i] + "##seg_" + std::to_string(i)).c_str());
				if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()))
				{
					seg_button_clicked = true;
					set_current_folder(std::vector<std::string>(cur_parent_folder.begin(), cur_parent_folder.begin() + i + 1));
				}
				if (i < cur_parent_folder.size() - 1)
				{
					ImGui::SameLine();
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + item_spacing);
					ImGui::Image(right_icon_tiny, ImVec2((float)right_icon_tiny.width, (float)right_icon_tiny.height));
				}
			}

			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !seg_button_clicked && ImGui::IsMouseHoveringRect(address_rect.Min, address_rect.Max))
			{
				text_edit_mode = true;
			}
			ImGui::PopStyleVar();
		}

		ImGui::PopStyleColor();

		//ImRect address_unused_rect = ImRect(ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMin().y), ImGui::GetWindowContentRegionMax());

		//ImDrawLists
		//ImGui::GetWindowDrawList()->AddRectFilled(address_rect.Min, address_rect.Max, (ImColor)ImVec4(1.0f, 0.0f, 1.0f, 0.2f));
		//ImGui::GetWindowDrawList()->AddRectFilled(address_unused_rect.Min, address_unused_rect.Max, (ImColor)ImVec4(1.0f, 1.0f, 0.0f, 0.2f));

		ImGui::EndMenuBar();
	}
}

bool FileBrowser::draw_footer()
{
	ImGui::Begin("footer", nullptr, 0);

	constexpr char file_name_prompt_text[] = "File name: ";
	char file_name_buffer[64] = {};
	std::vector<const char*> filter_names;
	float filters_size = 0.0f;

	strncpy_s(file_name_buffer, cur_filename.c_str(), cur_filename.size());

	if (filters.empty())
	{
		filters_size = 0.0f;
	}
	else
	{
		for (size_t i = 0; i < filters.size(); ++i)
		{
			filter_names.push_back(filters[i].first.c_str());
			filters_size = std::max(filters_size, ImGui::CalcTextSize(filters[i].first.c_str()).x);
		}
		filters_size += ImGui::CalcTextSize("A").y + ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetStyle().ItemSpacing.x;
	}

	float avail = ImGui::GetContentRegionAvail().x;
	float prompt_size = ImGui::CalcTextSize(file_name_prompt_text, file_name_prompt_text + sizeof(file_name_prompt_text)).x;
	float name_input_size = std::max(avail / 3.0f, 100.0f);
	float button_size = 80.0f;
	float total_top_len = prompt_size + name_input_size + filters_size + (!filters.empty() ? ImGui::GetStyle().ItemSpacing.x : 0.0f);
	float total_bottom_len = button_size * 2.0f + ImGui::GetStyle().ItemSpacing.x;

	ImGui::SetCursorPosX(avail - total_top_len);
	ImGui::PushStyleColor(ImGuiCol_Button, 0);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2());
	ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2());
	ImGui::Button(file_name_prompt_text);
	ImGui::SameLine();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);

	ImGui::SetNextItemWidth(name_input_size);
	if (ImGui::InputText("##file_name", file_name_buffer, sizeof(file_name_buffer)))
	{
		cur_filename = file_name_buffer;
	}

	if (!filter_names.empty())
	{
		ImGui::SameLine();

		static int selected_filter = (int)filter_index;
		ImGui::SetNextItemWidth(filters_size);
		if (ImGui::Combo("##filter_combo", &selected_filter, filter_names.data(), (int)filter_names.size()))
		{
			filter_index = selected_filter;
		}
	}

	ImGui::SetCursorPosX(avail - total_bottom_len);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);

	if (ImGui::Button("Open", ImVec2(button_size, 0.0f)))
	{
		bool is_valid = valid(IO::Join(cur_parent_folder) + "/" + cur_filename);

		if (!is_valid && !cur_parent_folder.empty() && cur_parent_folder.back() == cur_filename)
		{
			cur_parent_folder.pop_back();
			is_valid = valid(IO::Join(cur_parent_folder) + "/" + cur_filename);
		}

		bool parent_valid = valid(IO::Join(cur_parent_folder));

		if (require_existing_file && !is_valid)
		{
			popup_error("File does not exist");
		}
		else if (!parent_valid)
		{
			popup_error("Folder is not valid");
		}
		else if (confirm_overwrite && is_valid)
		{
			if (gui.confirm("Overwrite existing file(s)?"))
			{
				dialog_code = DialogCode_Confirm;
			}
		}
		else
		{
			dialog_code = DialogCode_Confirm;
		}
		if (dialog_code == DialogCode_Confirm)
		{
			GUI::thread_pool.stop();
			GUI::thread_pool.~thread_pool(); //library is buggy and doesn't allow pool reuse
			new(&GUI::thread_pool) ctpl::thread_pool(GUI::thread_count_max);
		}
	}

	ImGui::PopStyleVar();
	ImGui::SameLine();
	ImGui::SetNextItemWidth(button_size);

	if (ImGui::Button("Cancel", ImVec2(button_size, 0.0f)))
	{
		dialog_code = DialogCode_Cancel;
	}

	ImGui::End();

	return true;
}

void FileBrowser::load_pinned_folders()
{
	pinned_folders.clear();

	try
	{
		std::ifstream istream("pinned_folders.txt");
		istream.exceptions(std::ifstream::badbit); // No need to check failbit

		for (std::string line = ""; std::getline(istream, line);)
		{
			pinned_folders.push_back(line);
		}
	}
	catch (const std::exception&) {}

	if (pinned_folders.empty())
	{
		char buffer[512] = {};

		if (SHGetFolderPathA(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, buffer) == S_OK)
		{
			pinned_folders.push_back(buffer);
		}
		if (SHGetFolderPathA(NULL, CSIDL_DESKTOP, NULL, SHGFP_TYPE_CURRENT, buffer) == S_OK)
		{
			pinned_folders.push_back(buffer);
		}
	}

}

void FileBrowser::push_pinned_folder(const std::vector<std::string>& folder)
{
	pinned_folders.push_back(IO::Join(folder));
	save_pinned_folders();
}

void FileBrowser::erase_pinned_folder(size_t index)
{
	pinned_folders.erase(pinned_folders.begin() + index);
	save_pinned_folders();
}

void FileBrowser::save_pinned_folders()
{
	try
	{
		std::ofstream ostream("pinned_folders.txt");
		for (auto& path : pinned_folders)
		{
			ostream << path << std::endl;
		}
	}
	catch (const std::exception&)
	{
		gbl_err << "Failed to save pinned folders";
	}
}

void FileBrowser::set_current_folder(const std::vector<std::string>& folder)
{
	bool in = false, equal = false;
	if (compare(folder, cur_parent_folder, in, equal); equal)
	{
		return;
	}
	std::string folder_str = std::string(IO::Join(folder));
	if (folder.size() && folder_str.contains(".vbf"))
	{
		std::string arc_path, file_name;
		if (VBFUtils::Separate(folder_str, arc_path, file_name))
		{
			get_vbf(arc_path);
		}
	}

	folder_history_end = folder_history.insert(folder_history_end, folder) + 1;
	folder_history_end = folder_history.erase(folder_history_end, folder_history.end());
	cur_parent_folder = *(folder_history_end - 1);
	focus_current_folder = true;
}

void FileBrowser::set_current_folder_to_prev()
{
	if (!folder_history.empty())
	{
		if (folder_history_end > folder_history.begin() + 1)
		{
			folder_history_end--;
		}
		cur_parent_folder = *(folder_history_end - 1);
		focus_current_folder = true;
	}
}

void FileBrowser::set_current_folder_to_next()
{
	if (!folder_history.empty())
	{
		if (folder_history_end < folder_history.end())
		{
			folder_history_end++;
		}
		cur_parent_folder = *(folder_history_end - 1);
		focus_current_folder = true;
	}
}

