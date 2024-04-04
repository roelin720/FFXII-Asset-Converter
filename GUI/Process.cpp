#include "Process.h"
#include "ConverterInterface.h"
#include "Pipe.h"
#include <tchar.h>
#include <stdio.h> 
#include <strsafe.h>
#include <sstream>
#include <thread>
#include <iostream>

namespace 
{
    std::string LastErrorString()
    {
        LPVOID lpMsgBuf = nullptr;
        DWORD dw = GetLastError();

        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dw,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&lpMsgBuf,
            0, 
            NULL);

        return (char*)lpMsgBuf;
    }
}

int Process::RunConvertProcess(const std::string& cmd, std::list<std::pair<uint32_t, std::string>>& logs, const std::string& id)
{
    SECURITY_ATTRIBUTES saAttr = {};

    HANDLE hJob = CreateJobObject(NULL, NULL);
    if (hJob == NULL) 
    {
        std::cerr << "Failed to create Job Object. Error: " << GetLastError() << std::endl;
        return -1;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    if (!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli))) 
    {
        logs.push_back({ ERR.ID, "Failed to set Job Object information. Error: " + LastErrorString()});
        CloseHandle(hJob);
        return -1;
    }

    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    PipeServer pipe;
    if (!pipe.create("ConvPipe" + id))
    {
        logs.push_back({ ERR.ID, "Failed to create pipe" });
        return -1;
    }


    std::string full_cmd = cmd + " " + pipe.name + " " + EnumToString(GUITask::Convert);

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFOA siStartInfo;
    BOOL bSuccess = FALSE;

    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);

    if (!CreateProcessA(NULL, (char*)full_cmd.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo))
    {
        logs.push_back({ ERR.ID, "Failed to create process: " + LastErrorString() });
        return -1;
    }

    if (!AssignProcessToJobObject(hJob, piProcInfo.hProcess))
    {
        logs.push_back({ ERR.ID, "Failed to assign process to job object: " + LastErrorString() });
        return -1;
    }

    if (!pipe.connect())
    {
        logs.push_back({ ERR.ID, "Failed to connect pipe: " + LastErrorString() });
        return -1;
    }

    std::string log_buffer;
    log_buffer.reserve(1ull << 12);

    while (true)
    {   
        if (pipe.peek() > 0)
        {
            int32_t log_ID = 0;
            int32_t size = 0;
            if (!pipe.read(log_ID))
            {
                break;
            }

            if (log_ID < 0) //indicates end
            {
                pipe.write<uint32_t>(1); //acknowledgment
                break;
            }

            if (!pipe.read(size))
            {
                break;
            }
            log_buffer.resize(size);

            if (!pipe.read(log_buffer.data(), size))
            {
                break;
            }
            
            logs.push_back({ log_ID, log_buffer });
        }
    }

    if (WaitForSingleObject(piProcInfo.hProcess, 10000) == WAIT_TIMEOUT)
    {
        TerminateProcess(piProcInfo.hProcess, 1);
    }

    DWORD exit_code;
    GetExitCodeProcess(piProcInfo.hProcess, &exit_code);

    pipe.disconnect();
    pipe.close();

    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
    CloseHandle(hJob);

    return exit_code;
}

HANDLE Process::RunGUIProcess(GUITask task, std::string& pipe_name)
{
    HANDLE hJob = CreateJobObject(NULL, NULL);
    if (hJob == NULL) 
    {
        LOG(ERR) << "Failed to create Job Object. Error: " << GetLastError() << std::endl;
        return INVALID_HANDLE_VALUE;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    if (!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli))) 
    {
        LOG(ERR) << "Failed to set Job Object information. Error: " << GetLastError() << std::endl;
        CloseHandle(hJob);
        return INVALID_HANDLE_VALUE;
    }

    char app_path[1024] = {};
    DWORD app_path_size = GetModuleFileNameA(nullptr, app_path, 1024);

    std::string cmd = std::string(app_path) + " " + pipe_name + " " + EnumToString(task);

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFOA siStartInfo;
    BOOL bSuccess = FALSE;

    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);

    bool success = CreateProcessA(NULL, (char*)cmd.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo);

    if (!success)
    {
        LOG(ERR) << "Failed to create process" << std::endl;
        return INVALID_HANDLE_VALUE;
    }

    if (!AssignProcessToJobObject(hJob, piProcInfo.hProcess))
    {
        if (!TerminateProcess(piProcInfo.hProcess, 0))
        {
            LOG(ERR) << "Failed to terminate the process. Error: " << GetLastError() << std::endl;     
        }
        return INVALID_HANDLE_VALUE;
    }

    CloseHandle(piProcInfo.hThread);

    return piProcInfo.hProcess;
}

bool Process::TerminateGUIProcess(HANDLE& handle, DWORD timeout)
{
    if (handle == INVALID_HANDLE_VALUE)
    {
        return true;
    }
    if (timeout > 0)
    {
        WaitForSingleObject(handle, timeout);
    }
    DWORD exit_code = 0;
    if (GetExitCodeProcess(handle, &exit_code) && exit_code != STILL_ACTIVE)
    {
        return true;
    }

    if (!TerminateProcess(handle, 0))
    {
        LOG(ERR) << "Failed to terminate the process. Error: " << GetLastError() << std::endl;
        return false;
    }
    CloseHandle(handle);
    handle = INVALID_HANDLE_VALUE;
    return true;
}
