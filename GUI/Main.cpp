#include "ConverterInterface.h"
#include <Windows.h>
#include <fstream>
#include <io.h>
#include "GUI.h"
#include "VBFArchive.h"
#include "FileIOUtils.h"
#include <filesystem>
#include <windows.h>
#include "Process.h"
#include "Audio.h"

FILE* hfopen(const char* pipe_name)
{
	if (WaitNamedPipeA(TEXT(pipe_name), NMPWAIT_USE_DEFAULT_WAIT) == false)
	{
		return nullptr;
	}
	HANDLE hpipe = CreateFile(TEXT(pipe_name), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hpipe == INVALID_HANDLE_VALUE) return nullptr;
	
	int fd = _open_osfhandle(reinterpret_cast<intptr_t>(hpipe), 0);
	if (fd < 0) return nullptr;

	return _fdopen(fd, "w");
}

int main(int argc, char* argv[])
{
	std::string pipe_name = "";
	GUITask task = StringToEnum<GUITask>(argv[argc - 1]);
	task = task == GUITask::INVALID ? GUITask::Main : task;

	if (task == GUITask::Convert)
	{
		std::cout << "RUNNING PROCESS " << argv[argc - 4] << " " << argv[argc - 3] << " " << argv[argc - 2] << std::endl;

		struct LogRedirection
		{
			std::string name;
			FILE* pipe = nullptr;
			std::ofstream stream;
		} logs[3];

		for (size_t i = 0; i < 3; ++i)
		{
			logs[i].name = argv[argc - (4 - i)];
			logs[i].pipe = hfopen(logs[i].name.c_str());
			if (logs[i].pipe == nullptr)
			{
				gbl_err << "Failed to redirect " << logs[i].name << std::endl;
				return -1;
			}
			logs[i].stream = std::ofstream(logs[i].pipe);
		}

		gbl_log.ostream = &logs[0].stream;	
		gbl_warn.ostream = &logs[1].stream;
		gbl_err.ostream = &logs[2].stream;

		int err_code = -1;
		if (ConverterInterface::Initialise())
		{
			std::cout << "INITIALISED" << std::endl;

			err_code = !ConverterInterface::Run(argc - 4, (const char**)argv);
			ConverterInterface::Free();
		}

		for (size_t i = 0; i < 3; ++i)
		{
			logs[i].stream.flush();
			fclose(logs[i].pipe);
		}

		std::cout << "PROCESSING COMPLETE" << std::endl;
		return err_code;
	}
	else if (task != GUITask::Main)
	{
		pipe_name = argv[1];
	}

	int err_code = -1;	
	if (GUI gui; gui.init(task, pipe_name))
	{
		gui.run();
		gui.free();
		err_code = 0;
	}

	return err_code;
}