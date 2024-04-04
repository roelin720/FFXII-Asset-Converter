#include "ConverterInterface.h"
#include <Windows.h>
#include <fstream>
#include <io.h>
#include <fcntl.h>
#include "GUI.h"
#include "VBFArchive.h"
#include "FileIOUtils.h"
#include <filesystem>
#include <windows.h>
#include "Process.h"
#include "Audio.h"

FILE* hfopen(const char* pipe_name)
{
	if (WaitNamedPipeA(TEXT(pipe_name), NMPWAIT_WAIT_FOREVER) == false)
	{
		return nullptr;
	}
	HANDLE hpipe = CreateFile(TEXT(pipe_name), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hpipe == INVALID_HANDLE_VALUE) return nullptr;
	
	int fd = _open_osfhandle(reinterpret_cast<intptr_t>(hpipe), 0);
	if (fd < 0) return nullptr;

	return _fdopen(fd, "wb");
}

int main(int argc, char* argv[])
{
	std::string pipe_name = "";
	GUITask task = StringToEnum<GUITask>(argv[argc - 1]);
	task = task == GUITask::INVALID ? GUITask::Main : task;

	if (task == GUITask::Convert)
	{
		pipe_name = argv[argc - 2];

		std::cout << "RUNNING PROCESS with pipe " << pipe_name << std::endl;

		PipeClient pipe;
		if (!pipe.open(pipe_name, NMPWAIT_WAIT_FOREVER))
		{
			return -1;
		}

		int fd = _open_osfhandle(reinterpret_cast<intptr_t>(pipe.handle), 0);
		if (fd < 0)
		{
			std::cerr << "Failed to get file descriptor from pipe " << pipe_name << std::endl;
			return -1;
		}

		GlobalLogger::file = _fdopen(fd, "w+b");
		if (GlobalLogger::file == nullptr)
		{
			std::cerr << "Failed to get FILE from file descriptor for pipe " << pipe_name << std::endl;
			return -1;
		}

		int err_code = -1;
		if (ConverterInterface::Initialise())
		{
			std::cout << "INITIALISED" << std::endl;

			err_code = !ConverterInterface::Run(argc - 2, (const char**)argv);
			ConverterInterface::Free();
		}

		if (pipe.write<int32_t>(-1)) //indicates end
		{
			uint32_t acknowledgment = 0;
			pipe.read(acknowledgment);
		}

		fflush(GlobalLogger::file);
		fclose(GlobalLogger::file);
		GlobalLogger::file = stderr;

		pipe.close();

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