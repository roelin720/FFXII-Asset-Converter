#pragma once
#include <Windows.h>
#include "VBFArchive.h"
#include "ClipDropTarget.h"

class FileBrowser;
class ClipDragDrop
{
public:
	static HGLOBAL CreatePathListHGLOBAL(const std::vector<std::string>& paths);

	static HRESULT RunDragDrop(FileBrowser& browser, bool left_click);
};