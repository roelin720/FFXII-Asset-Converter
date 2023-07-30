#pragma once
#include "FileIOUtils.h"
#include "VBFArchive.h"
#include "Pipe.h"
#include <string>
#include <vector>

class FileBrowser;
namespace GUIMessagePayload
{
	struct WindowInit;
	struct Progress;
}

class VBFClipHelper
{
public:
	FileBrowser& browser;

	std::mutex progress_mutex;
	std::thread progress_message_thread;
	PipeServer progress_pipe;
	HANDLE progress_handle = INVALID_HANDLE_VALUE;
	bool stop_progress_message_thread = false;
	bool stop_operation = false;
	GUIMessagePayload::Progress* progress = nullptr;
	GUIMessagePayload::WindowInit* window_init = nullptr;

	virtual ~VBFClipHelper();

	void reset();

	bool get_current_vbf_data(VBFArchive*& vbf, VBFArchive::TreeNode*& node);
	bool configure_progress_window_init(int width, int height);
	bool launch_progress_window(const std::string& pipe_name, int width, int height);
	bool launch_progress_message_thread();
	bool post_progress();

	void update_progress(const std::string& name, int elapsed, int dest, bool cancel_disabled = false);
	bool ended_early();
	void end_operation();

	void handle_exception(const std::exception& e);

protected:
	VBFClipHelper(FileBrowser& browser);
};

class VBFExtractClipHelper : public VBFClipHelper
{
public:
	std::string extract_dir;

	void CreateFolderTree(const std::string& parent_path, const VBFArchive::TreeNode* node);
	void GatherFilePaths(std::list<std::pair<const VBFArchive::TreeNode*, std::string>>& file_paths, std::list<std::pair<const VBFArchive::TreeNode*, std::string>>& folder_paths, const std::string& parent_path, const VBFArchive::TreeNode* node);

	VBFExtractClipHelper(FileBrowser& browser, const std::string& extract_dir);

	bool perform_extractions();
};

class VBFInjectClipHelper : public VBFClipHelper
{
public:
	VBFInjectClipHelper(FileBrowser& browser);

	bool perform_injections(const std::vector<std::wstring>& dropped_paths);
};