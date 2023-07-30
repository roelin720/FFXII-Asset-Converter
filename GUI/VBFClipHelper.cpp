#include "VBFClipHelper.h"
#include "ConverterInterface.h"
#include <ShlObj.h>
#include "GUI.h"
#include "GUIBrowser.h"
#include "Process.h"
#include "FileIOUtils.h"
#include <locale>
#include <codecvt>
#include <Shlwapi.h>

VBFExtractClipHelper::VBFExtractClipHelper(FileBrowser& browser, const std::string& extract_dir)
	: VBFClipHelper(browser), extract_dir(extract_dir)
{}

void VBFExtractClipHelper::CreateFolderTree(const std::string& parent_path, const VBFArchive::TreeNode* node)
{
	if (!node->children.empty())
	{
		std::string path = parent_path + "/" + node->name_segment;

		std::filesystem::create_directories(path);

		for (size_t i = 0; i < node->children.size(); ++i)
		{
			CreateFolderTree(path, node->children[i]);
		}
	}
}

void VBFExtractClipHelper::GatherFilePaths(std::list<std::pair<const VBFArchive::TreeNode*, std::string>>& file_paths, std::list<std::pair<const VBFArchive::TreeNode*, std::string>>& folder_paths, const std::string& parent_path, const VBFArchive::TreeNode* node)
{
	std::string path = parent_path + "/" + node->name_segment;

	if (node->children.empty())
	{
		file_paths.push_back({ node, path });
	}
	else
	{
		folder_paths.push_back({ node, path });

		for (size_t i = 0; i < node->children.size(); ++i)
		{
			GatherFilePaths(file_paths, folder_paths, path, node->children[i]);
		}
	}
}

bool VBFExtractClipHelper::perform_extractions()
{
	try
	{
		reset();

		VBFArchive* vbf = nullptr;
		VBFArchive::TreeNode* node = nullptr;

		if (!get_current_vbf_data(vbf, node) ||
			!launch_progress_window("DropSourceProgress", 400, 100) ||
			!launch_progress_message_thread()
		) {
			browser.gui.alert("Failed to begin drag-drop extraction");
			return false;
		}

		std::list<std::pair<const VBFArchive::TreeNode*, std::string>> paths_list;
		std::list<std::pair<const VBFArchive::TreeNode*, std::string>> folder_list;
		
		update_progress("Preparing for extraction...", 1, 1);
		GatherFilePaths(paths_list, folder_list, extract_dir, node);

		size_t i = 0;
		for (auto& folder : folder_list)
		{
			if (ended_early()) return false;

			update_progress("Extracting folder hierarchy...", i, folder_list.size());
			std::filesystem::create_directory(folder.second);
			++i;
		}

		i = 0;
		for (auto& path : paths_list)
		{
			if (stop_operation)
			{
				update_progress("Cancelling drag-drop...", i, paths_list.size(), true);
				post_progress();
				//don't call end up operation to keep message on screen until delete is called
				return false;
			}

			update_progress("Extracting file:\n" + path.first->name_segment + "...", i, paths_list.size());

			if (!vbf->extract(path.first->full_name(), path.second))
			{
				gbl_log << "Failed to extact " << path.first->full_name() << std::endl;
			}
			++i;
		}

		update_progress("Moving extracted files...", i, paths_list.size(), true);
		//don't call end up operation to keep message on screen until delete is called
	}
	catch (std::exception& e)
	{
		handle_exception(e);
		return false;
	}

	return true;
}

VBFInjectClipHelper::VBFInjectClipHelper(FileBrowser& browser) 
	: VBFClipHelper(browser)
{}

bool VBFInjectClipHelper::perform_injections(const std::vector<std::wstring>& dropped_paths)
{
	try
	{
		reset();

		VBFArchive* vbf = nullptr;
		VBFArchive::TreeNode* node = nullptr;
		GUIMessagePayload::WindowInit window_init;
		GUIMessagePayload::Progress progress;

		if (!get_current_vbf_data(vbf, node) ||
			!launch_progress_window("DropTargetProgress", 400, 100) ||
			!launch_progress_message_thread()
		){
			browser.gui.alert("Failed to begin drag-drop injection");
			return false;
		}

		struct DroppedFile
		{
			std::wstring full_path;
			std::wstring root;
			std::string stem;
		};

		std::list<DroppedFile> dropped_files;

		size_t i = 0;
		for (const std::wstring path : dropped_paths)
		{
			if (ended_early()) return false;

			update_progress("Gathering dropped files...", i, dropped_paths.size());

			if (!std::filesystem::exists(path))
			{
				continue;
			}

			try
			{
				if (std::filesystem::is_directory(path))
				{
					for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
					{
						if (std::filesystem::exists(entry) && entry.is_regular_file())
						{
							dropped_files.push_back(
							DroppedFile{
								.full_path = entry.path().wstring(),
								.root = path,
								.stem = (std::filesystem::path(path).filename() / std::filesystem::relative(entry, path)).string()
							});
						}
					}
				}
				else
				{
					dropped_files.push_back(
					DroppedFile{
						.full_path = path,
						.root = std::filesystem::path(path).parent_path(),
						.stem = std::filesystem::path(path).filename().string()
					});
				}
			}
			catch (const std::exception& e)
			{
				std::wcerr << "Failed to drop file/folder " << path << " - " << e.what();
			}
			++i;
		}

		if (dropped_files.empty())
		{
			end_operation();
			browser.gui.alert("Failed to find any files from drop source(s).");
			return false;
		}

		std::vector<std::pair<std::string, std::wstring>> file_map;
		file_map.reserve(dropped_files.size());

		i = 0;
		for (const DroppedFile& file : dropped_files)
		{
			if (ended_early()) return false;

			update_progress("Finding corresponding vbf files...", i, dropped_files.size());

			std::string asset_path = IO::Normalise(node->full_name() + "/" + file.stem);
			if (vbf->find_file(asset_path) != nullptr)
			{
				file_map.push_back({ asset_path, file.full_path });
			}
			++i;
		}

		if (file_map.empty())
		{
			end_operation();
			browser.gui.alert("Failed to find any files that correspond to vbf assets.\nPlease make sure file names and locations match.");
			return false;
		}

		int success_count = 0;
		i = 0;
		for (const auto& mapping : file_map)
		{
			if (stop_operation) break;

			update_progress("Injecting asset:\n" + IO::FileName(mapping.first), i, file_map.size());

			if (vbf->inject(mapping.first, mapping.second))
			{
				++success_count;
			}
			++i;
		}
		if (success_count > 0)
		{
			update_progress("Updating VBF header...", 1, 1, true);
			vbf->update_header();
		}

		end_operation();
		browser.gui.alert(std::to_string(success_count) + "/" + std::to_string(file_map.size()) + " files injected");
	}
	catch (std::exception& e)
	{
		handle_exception(e);
		return false;
	}

	reset();
	return true;
}

VBFClipHelper::VBFClipHelper(FileBrowser& browser) : browser(browser) {}

VBFClipHelper::~VBFClipHelper()
{
	reset();
}

void VBFClipHelper::reset()
{
	end_operation();
	stop_progress_message_thread = false;
	stop_operation = false;
}

bool VBFClipHelper::get_current_vbf_data(VBFArchive*& vbf, VBFArchive::TreeNode*& node)
{
	std::string cur_folder_str = IO::Join(browser.cur_parent_folder);

	std::string arc_path, arc_asset_path;
	if (!VBFUtils::Separate(cur_folder_str, arc_path, arc_asset_path))
	{
		browser.gui.alert("Current path into vbf is invalid");
		return false;
	}

	if (auto find = browser.archives.find(arc_path);
		find != browser.archives.end() &&
		find->second.loaded &&
		find->second.valid
		) {
		vbf = &find->second.archive;
	}
	else return false;

	node = vbf->find_node(arc_asset_path);
	if (node == nullptr)
	{
		browser.gui.alert("Current path into vbf is invalid");
		return false;
	}

	return true;
}

bool VBFClipHelper::configure_progress_window_init(int width, int height)
{
	window_init = new GUIMessagePayload::WindowInit();

	window_init->hints[0] = { GLFW_DECORATED, GLFW_FALSE };
	window_init->hints[1] = { GLFW_FLOATING, GLFW_TRUE };
	window_init->parent_window = browser.window;

	RECT parent_rect;
	if (!GetWindowRect(browser.window, &parent_rect))
	{
		return false;
	}
	int parent_width = parent_rect.right - parent_rect.left;
	int parent_height = parent_rect.bottom - parent_rect.top;
	int parent_x = parent_rect.left;
	int parent_y = parent_rect.top;

	window_init->width = width;
	window_init->height = height;
	window_init->pos_x = (parent_width - window_init->width) / 2;
	window_init->pos_y = (parent_height - window_init->height) / 2;

	strcpy_s(window_init->title, window_init->title_max_len, "Progress");

	return true;
}

bool VBFClipHelper::launch_progress_window(const std::string& pipe_name, int width, int height)
{
	if (!configure_progress_window_init(width, height))
	{
		browser.gui.alert("Failed to initialise progress window");
		return false;
	}

	if (!progress_pipe.create("DragDstProgress" + std::to_string(GetCurrentProcessId())))
	{
		browser.gui.alert("Failed to create progress window for drag-drop");
		return false;
	}
	progress_handle = Process::RunGUIProcess(GUITask::Progress, progress_pipe.name);
	if (progress_handle == INVALID_HANDLE_VALUE)
	{
		browser.gui.alert("Failed to connect to create progress window for drag-drop");
		Process::TerminateGUIProcess(progress_handle);
		return false;
	}
	if (!progress_pipe.connect())
	{
		browser.gui.alert("Failed to connect to connect to client progress_pipe for drag-drop");
		Process::TerminateGUIProcess(progress_handle);
		return false;
	}
	if (!progress_pipe.write(GUIMessage::WindowInit) || !progress_pipe.write(*window_init))
	{
		browser.gui.alert("Failed to connect to write to client progress_pipe for drag-drop");
		Process::TerminateGUIProcess(progress_handle);
		return false;
	}
	progress = new GUIMessagePayload::Progress();
	return true;
}

bool VBFClipHelper::launch_progress_message_thread()
{
	progress_message_thread = std::thread([this]()
	{
		while (!stop_progress_message_thread)
		{
			if (progress_pipe.peek() > 0)
			{
				GUIMessage message;
				if (!progress_pipe.read(message))
				{
					Process::TerminateGUIProcess(progress_handle);
					stop_operation = true;
					return;
				}
				if (message == GUIMessage::Exit)
				{
					stop_operation = true;
				}
				else if (message == GUIMessage::Request)
				{
					if (!post_progress())
					{
						Process::TerminateGUIProcess(progress_handle);
						stop_operation = true;
						return;
					}
				}
			}
			Sleep(50);
		}
	});

	return true;
}

bool VBFClipHelper::post_progress()
{
	progress_mutex.lock();
	GUIMessagePayload::Progress progress_cpy = *progress;
	progress_mutex.unlock();

	if (!progress_pipe.write(GUIMessage::Progress) || !progress_pipe.write(progress_cpy))
	{
		return false;
	}
	return true;
}

void VBFClipHelper::update_progress(const std::string& name, int elapsed, int dest, bool cancel_disabled)
{
	progress_mutex.lock();

	strcpy_s(progress->label, progress->label_max_len, name.c_str());
	progress->cancel_disabled = cancel_disabled;
	progress->elpased = elapsed;
	progress->destination = dest;

	progress_mutex.unlock();
}

bool VBFClipHelper::ended_early()
{
	if (stop_operation)
	{
		stop_progress_message_thread = true;
		if (progress_message_thread.joinable())
		{
			progress_message_thread.join();
		}
		if (progress_pipe.opened())
		{
			progress_pipe.write(GUIMessage::Exit);
			progress_pipe.close();
		}
		Process::TerminateGUIProcess(progress_handle, 2000);
		delete progress;
		progress = nullptr;
		delete window_init;
		window_init = nullptr;
		return true;
	}
	return false;
}

void VBFClipHelper::end_operation()
{
	stop_operation = true;
	ended_early();
}

void VBFClipHelper::handle_exception(const std::exception& e)
{
	std::string error_message = std::string("Error occured when dropping files - ") + e.what();
	gbl_err << error_message << std::endl;
	browser.gui.alert(error_message);
}
