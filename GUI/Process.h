#pragma once
#include <Windows.h>
#include "Enums.h"
#include <string>
#include <list>

namespace Process
{
	int RunConvertProcess(const std::string& cmd, std::list<std::pair<uint32_t, std::string>>& logs, const std::string& id);
	HANDLE RunGUIProcess(GUITask task, std::string& pipe_name);
	bool TerminateGUIProcess(HANDLE& handle, DWORD timeout = 0);
}