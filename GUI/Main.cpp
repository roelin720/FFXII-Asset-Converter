#include <iostream>
#include <Windows.h>
#include <PhyreInterface.h>
#include <fstream>
#include <io.h>
#include "PhyreGUI.h"
#include "VBFArchive.h"
#include "PhyreIOUtils.h"
#include <filesystem>
#include <windows.h>
#include "Audio.h"

FILE* hfopen(const char* pipe_name)
{
	if (WaitNamedPipeA(TEXT(pipe_name), NMPWAIT_USE_DEFAULT_WAIT) == false)
	{
		return nullptr;
	}
	HANDLE hpipe = CreateFile(TEXT(pipe_name),
		GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	if (hpipe == INVALID_HANDLE_VALUE) return nullptr;
	int fd = _open_osfhandle(reinterpret_cast<intptr_t>(hpipe), 0);
	if (fd < 0) return nullptr;
	return _fdopen(fd, "w");
}

void test(VBFArchive& archive, std::string orig_parent, std::string orig_file, std::string new_file)
{
	std::string local_file = std::filesystem::relative(orig_file, orig_parent).string();
	std::replace(local_file.begin(), local_file.end(), '\\', '/');
	if (!archive.extract(local_file, new_file))
	{
		std::cerr << "Extract Failed - " << local_file << std::endl;
		if (std::filesystem::exists(new_file)) std::remove(new_file.c_str());
		return;
	}
	std::ifstream file1(orig_file, std::ifstream::ate | std::ifstream::binary); //open file at the end
	std::ifstream file2(new_file, std::ifstream::ate | std::ifstream::binary); //open file at the end
	const std::ifstream::pos_type fileSize = file1.tellg();

	if (fileSize != file2.tellg())
	{
		std::cerr << "Different Sizes - " << local_file << std::endl;
		file1.close();
		file2.close();
		std::remove(new_file.c_str());
		return;
	}

	file1.seekg(0);
	file2.seekg(0);

	std::istreambuf_iterator<char> begin1(file1);
	std::istreambuf_iterator<char> begin2(file2);

	if (!std::equal(begin1, std::istreambuf_iterator<char>(), begin2))
	{
		std::cerr << "Different Content - " << local_file << std::endl;
		file1.close();
		file2.close();
		std::remove(new_file.c_str());
	}

	file1.close();
	file2.close();
	std::remove(new_file.c_str());
}

void test(VBFArchive& archive, std::string dir)
{
	using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;

	for (const auto& entry : recursive_directory_iterator(dir))
	{
		if (entry.is_regular_file() && std::filesystem::exists(entry.path()))
		{
			test(archive, dir,
				entry.path().string(),
				PhyreIO::TmpPath() + "/" + entry.path().filename().string()
			);
		}
	}
}

int main(int argc, char* argv[])
{	
	if (argc >= 6)
	{
		std::cout << "RUNNING PROCESS " << argv[argc - 2] << " " << argv[argc - 1] << std::endl;

		FILE* log_pipe = hfopen(argv[argc - 2]);
		if (log_pipe == nullptr)
		{
			std::cerr << "Failed to redirect cout stream" << std::endl;
			return -1;
		}
		FILE* err_pipe = hfopen(argv[argc - 1]);
		if (err_pipe == nullptr)
		{
			std::cerr << "Failed to redirect cerr stream" << std::endl;
			return -1;
		}
		std::ofstream log_stream(log_pipe);
		std::streambuf* old_clog = std::clog.rdbuf(log_stream.rdbuf());
		
		std::ofstream err_stream(err_pipe); 
		std::streambuf* old_cerr = std::cerr.rdbuf(err_stream.rdbuf());

		int err_code = -1;

		if (PhyreInterface::Initialise())
		{
			std::cout << "INITIALISED" << std::endl;

			err_code = !PhyreInterface::Run(argc - 2, (const char**)argv);
			PhyreInterface::Free();
		}

		log_stream.flush();
		err_stream.flush();
		fclose(log_pipe);
		fclose(err_pipe);
		std::clog.rdbuf(old_clog);
		std::cerr.rdbuf(old_cerr);
		std::cout << "PROCESSING COMPLETE" << std::endl;
		return err_code;
	}

	if ((CoInitialize(NULL) != S_OK)) //needed for audio management
	{
		return -1;
	}
	int err_code = !Phyre::GUI().run();
	CoUninitialize();
	return err_code;
}