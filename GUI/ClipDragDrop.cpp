#include "ClipDragDrop.h"
#include "ConverterInterface.h"
#include "GUI.h"
#include "ClipDataObject.h"
#include "ClipDropSource.h"
#include "ClipDropTarget.h"
#include <ShlObj.h>

HGLOBAL ClipDragDrop::CreatePathListHGLOBAL(const std::vector<std::string>& files)
{
	size_t buffer_size = sizeof(DROPFILES) + 1;

	for (const std::string& file : files)
	{
		buffer_size += strlen(file.c_str()) + 1;
	}

	HGLOBAL hglobal = GlobalAlloc(GHND, buffer_size);

	if (hglobal == NULL)
	{
		return NULL;
	}

	DROPFILES* buffer = (DROPFILES*)GlobalLock(hglobal);

	if (buffer == NULL)
	{
		GlobalFree(hglobal);
		return NULL;
	}

	buffer->pFiles = sizeof(DROPFILES);
	buffer->fWide = false;

	char* name_ptr = (char*)(LPBYTE(buffer) + sizeof(DROPFILES));

	for (const std::string& file : files)
	{
		size_t len = strlen(file.c_str()) + 1;
		memcpy(name_ptr, file.c_str(), len);
		name_ptr += len;
	}
	*name_ptr = '\0';

	if (GlobalUnlock(hglobal) != 0)
	{
		GlobalFree(hglobal);
		return NULL;
	}

	return hglobal;
}

HRESULT ClipDragDrop::RunDragDrop(FileBrowser& browser, bool left_click)
{
	VBFArchive* vbf = nullptr;
	VBFArchive::TreeNode* node = nullptr;
	std::string cur_folder_str = IO::Join(browser.cur_parent_folder);

	std::string tmp_dir;
	std::string drag_path;

	VBFExtractClipHelper* vbf_helper = nullptr;

	DWORD drop_effect = 0;

	if (!browser.cur_filename.empty() && cur_folder_str.contains(".vbf"))
	{
		drop_effect = DROPEFFECT_MOVE;
		tmp_dir = IO::CreateTmpPath("tmp_vbf_drag_drop");
		if (tmp_dir.empty())
		{
			gbl_err << "Failed to create temp path" << tmp_dir << std::endl;
			return DRAGDROP_S_CANCEL;
		}

		std::string arc_path, arc_asset_path;
		if (!VBFUtils::Separate(cur_folder_str, arc_path, arc_asset_path))
		{
			gbl_err << "current path into vbf is invalid" << std::endl;
			return DRAGDROP_S_CANCEL;
		}

		if (auto find = browser.archives.find(arc_path);
			find != browser.archives.end() &&
			find->second.loaded &&
			find->second.valid
			) {
			vbf = &find->second.archive;
		}
		else return DRAGDROP_S_CANCEL;

		node = vbf->find_node(arc_asset_path);
		if (node == nullptr)
		{
			gbl_err << "current path into vbf is invalid" << std::endl;
			return DRAGDROP_S_CANCEL;
		}

		drag_path = tmp_dir + "/" + node->name_segment;

		vbf_helper = new VBFExtractClipHelper(browser, tmp_dir);
	}
	else
	{
		drop_effect = DROPEFFECT_COPY | DROPEFFECT_MOVE;
		drag_path = cur_folder_str;

		if (!browser.cur_filename.empty())
		{
			drag_path += "/" + browser.cur_filename;
		}
	}

	HGLOBAL hglobal = CreatePathListHGLOBAL({drag_path});
	if (hglobal == NULL)
	{
		return DRAGDROP_S_CANCEL;
	}

	STGMEDIUM medium = { 0 };
	medium.tymed = TYMED_HGLOBAL;
	medium.hGlobal = hglobal;
	medium.pUnkForRelease = NULL;

	FORMATETC format = { 0 };
	format.cfFormat = CF_HDROP;
	format.tymed = TYMED_HGLOBAL;
	format.ptd = NULL;
	format.dwAspect = DVASPECT_CONTENT;
	format.lindex = -1;

	ClipDropSource* drop_source = new ClipDropSource(left_click ? MK_LBUTTON : MK_RBUTTON, browser.drop_target);
	ClipDataObject* data_object = new ClipDataObject({{format, medium}});

	drop_source->vbf_helper = vbf_helper;

	DWORD drop_effect_result = 0;
	HRESULT hr = DoDragDrop(data_object, drop_source, drop_effect, &drop_effect_result);

	delete vbf_helper;
	drop_source->vbf_helper = nullptr;

	drop_source->Release();
	data_object->Release();

	if (!tmp_dir.empty())
	{
		std::filesystem::remove_all(tmp_dir);
	}

	return hr;
}