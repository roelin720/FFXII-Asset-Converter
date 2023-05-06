#include "Process.h"
#include <tchar.h>
#include <stdio.h> 
#include <strsafe.h>
#include <sstream>
#include <thread>
#include <iostream>

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

#define PIPE_TIMEOUT  1000
//#define ErrorExit(x) {__debugbreak();}

int Process::Execute(const std::string& cmd, std::string& cout, std::string& cerr, const std::string& id)
{
    SECURITY_ATTRIBUTES saAttr = {};

    printf("\n->Start of parent execution.\n");

    // Set the bInheritHandle flag so pipe handles are inherited. 

    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    std::string out_pipe_name = "\\\\.\\pipe\\stdOut" + id;
    HANDLE out_pipe = CreateNamedPipeA(out_pipe_name.c_str(),
        PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
        1, 1 << 16, 1 << 16, PIPE_TIMEOUT, &saAttr);
    if (out_pipe == INVALID_HANDLE_VALUE)
    {
        ErrorExit(TEXT("CreatePipe stdOut"));
        return -1;
    }

    std::string err_pipe_name = "\\\\.\\pipe\\stdErr" + id;
    HANDLE err_pipe = CreateNamedPipeA(err_pipe_name.c_str(),
        PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
        1, 1 << 16, 1 << 16, PIPE_TIMEOUT, &saAttr);
    if (err_pipe == INVALID_HANDLE_VALUE)
    {
        ErrorExit(TEXT("CreatePipe stdErr"));
        return -1;
    }

    std::string full_cmd = cmd + " " + out_pipe_name + " " + err_pipe_name;

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFOA siStartInfo;
    BOOL bSuccess = FALSE;

    // Set up members of the PROCESS_INFORMATION structure. 

    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    // Set up members of the STARTUPINFO structure. 
    // This structure specifies the STDIN and STDOUT handles for redirection.

    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);

    // Create the child process. 

    bSuccess = CreateProcessA(NULL,
        (char*)full_cmd.c_str(),     // command line 
        NULL,          // process security attributes 
        NULL,          // primary thread security attributes 
        TRUE,          // handles are inherited 
        0,             // creation flags 
        NULL,          // use parent's environment 
        NULL,          // use parent's current directory 
        &siStartInfo,  // STARTUPINFO pointer 
        &piProcInfo);  // receives PROCESS_INFORMATION 

    if (!bSuccess)
    {
        ErrorExit(TEXT("CreateProcess"));
        return -1;
    }

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

    if (ConnectNamedPipe(out_pipe, NULL) == FALSE && (GetLastError() != ERROR_PIPE_CONNECTED))
    {
        ErrorExit(TEXT("ConnectNamedPipe out"));
        return -1;
        //std::cout << "FAILED TO CONNECT COUT" << std::endl;
    }
    if (ConnectNamedPipe(err_pipe, NULL) == FALSE && (GetLastError() != ERROR_PIPE_CONNECTED))
    {
        ErrorExit(TEXT("ConnectNamedPipe err"));
        return -1;
        //std::cout << "FAILED TO CONNECT CERR" << std::endl;
    }

    cout = get_pipe_str(out_pipe);
    cerr = get_pipe_str(err_pipe);
    DisconnectNamedPipe(out_pipe);
    DisconnectNamedPipe(err_pipe);

    if (WaitForSingleObject(piProcInfo.hProcess, 10000) == WAIT_TIMEOUT)
    {
        TerminateProcess(piProcInfo.hProcess, 1);
    }
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);

    CloseHandle(out_pipe);
    CloseHandle(err_pipe);

    return true;
}