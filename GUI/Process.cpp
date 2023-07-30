#include "Process.h"
#include "ConverterInterface.h"
#include <tchar.h>
#include <stdio.h> 
#include <strsafe.h>
#include <sstream>
#include <thread>
#include <iostream>

namespace {

    constexpr size_t buffSize = 4096;

    void ErrorExit(const char* lpszFunction)
    {
        LPVOID lpMsgBuf = nullptr;
        LPVOID lpDisplayBuf = nullptr;
        DWORD dw = GetLastError();

        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dw,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&lpMsgBuf,
            0, NULL);

        lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
            (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
        StringCchPrintf((LPTSTR)lpDisplayBuf,
            LocalSize(lpDisplayBuf) / sizeof(TCHAR),
            TEXT("%s failed with error %d: %s"),
            lpszFunction, dw, lpMsgBuf);
        MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

        LocalFree(lpMsgBuf);
        LocalFree(lpDisplayBuf);
        ExitProcess(1);
    }
}
#define PIPE_TIMEOUT  1000
#define BUFSIZE 1024
//#define ErrorExit(x) {__debugbreak();}

int Process::RunConvertProcess(const std::string& cmd, std::string& out, std::string& warn, std::string& err, const std::string& id)
{
    SECURITY_ATTRIBUTES saAttr = {};

    HANDLE hJob = CreateJobObject(NULL, NULL);
    if (hJob == NULL) {
        std::cerr << "Failed to create Job Object. Error: " << GetLastError() << std::endl;
        return 1;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    if (!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli))) {
        std::cerr << "Failed to set Job Object information. Error: " << GetLastError() << std::endl;
        CloseHandle(hJob);
        return 1;
    }

    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    struct Pipe
    {
        std::string name;
        HANDLE handle;
    };
    
    Pipe pipes[3] =
    {
        {"\\\\.\\pipe\\out" + id},
        {"\\\\.\\pipe\\warn" + id},
        {"\\\\.\\pipe\\err" + id}
    };

    for (Pipe& pipe : pipes)
    {
        pipe.handle = CreateNamedPipeA(pipe.name.c_str(),
            PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
            1, 1 << 16, 1 << 16, PIPE_TIMEOUT, &saAttr);
        if (pipe.handle == INVALID_HANDLE_VALUE)
        {
            ErrorExit(TEXT(("CreatePipe " + pipe.name).c_str()));
            return -1;
        }
    }

    std::string full_cmd = cmd + " " + pipes[0].name + " " + pipes[1].name + " " + pipes[2].name + " " + EnumToString(GUITask::Convert);

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFOA siStartInfo;
    BOOL bSuccess = FALSE;

    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);

    // Create the child process. 

    bSuccess = CreateProcessA(NULL, (char*)full_cmd.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo);

    if (!bSuccess)
    {
        ErrorExit(TEXT("CreateProcess"));
        return -1;
    }

    AssignProcessToJobObject(hJob, piProcInfo.hProcess);

    auto get_pipe_str = [](HANDLE pipe)
    {
        DWORD dwRead;
        CHAR chBuf[buffSize] = {};
        BOOL bSuccess = FALSE;
        std::stringstream sstream;

        for (;;)
        {
            bSuccess = ReadFile(pipe, chBuf, buffSize - 1, &dwRead, NULL);
            if (!bSuccess) break;
            chBuf[dwRead] = '\0';

            sstream << std::string(chBuf, chBuf + dwRead);
        }
        return sstream.str();
    };

    for (Pipe& pipe : pipes)
    {
        if (ConnectNamedPipe(pipe.handle, NULL) == FALSE && (GetLastError() != ERROR_PIPE_CONNECTED))
        {
            ErrorExit(TEXT(("ConnectNamedPipe " + pipe.name).c_str()));
            return -1;
        }
    }

    out = get_pipe_str(pipes[0].handle);
    warn = get_pipe_str(pipes[1].handle);
    err = get_pipe_str(pipes[2].handle);

    for (Pipe& pipe : pipes) DisconnectNamedPipe(pipe.handle);

    if (WaitForSingleObject(piProcInfo.hProcess, 10000) == WAIT_TIMEOUT)
    {
        TerminateProcess(piProcInfo.hProcess, 1);
    }

    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
    CloseHandle(hJob);

    for (Pipe& pipe : pipes) CloseHandle(pipe.handle);

    return 0;
}

HANDLE Process::RunGUIProcess(GUITask task, std::string& pipe_name)
{
    HANDLE hJob = CreateJobObject(NULL, NULL);
    if (hJob == NULL) 
    {
        std::cerr << "Failed to create Job Object. Error: " << GetLastError() << std::endl;
        return INVALID_HANDLE_VALUE;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    if (!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli))) {
        std::cerr << "Failed to set Job Object information. Error: " << GetLastError() << std::endl;
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
        __debugbreak();
        ErrorExit(TEXT("CreateProcess"));
        return INVALID_HANDLE_VALUE;
    }

    if (!AssignProcessToJobObject(hJob, piProcInfo.hProcess))
    {
        if (!TerminateProcess(piProcInfo.hProcess, 0))
        {
            gbl_err << "Failed to terminate the process. Error: " << GetLastError() << std::endl;     
        }
        __debugbreak();
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
        gbl_err << "Failed to terminate the process. Error: " << GetLastError() << std::endl;
        return false;
    }
    CloseHandle(handle);
    handle = INVALID_HANDLE_VALUE;
    return true;
}
