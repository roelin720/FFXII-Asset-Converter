#pragma once
#include <Windows.h>
#include "Enums.h"
#include <string>

namespace Process
{
	int RunConvertProcess(const std::string& cmd, std::string& out, std::string& warn, std::string& err, const std::string& id);
	HANDLE RunGUIProcess(GUITask task, std::string& pipe_name);
	bool TerminateGUIProcess(HANDLE& handle, DWORD timeout = 0);
}