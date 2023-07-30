#include "Pipe.h"
#include "ConverterInterface.h"
#include <list>

bool PipeServer::create(const std::string& name, uint32_t buffer_size)
{
    this->name = name;
    this->buffer_size = buffer_size;

    gbl_log << "Opening pipe " << name << std::endl;

    SECURITY_ATTRIBUTES saAttr = {};

    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    handle = CreateNamedPipe(make_ID().c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1, buffer_size, buffer_size, 1000, &saAttr);

    if (handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    return true;
}

bool PipeServer::connect()
{
    if (ConnectNamedPipe(handle, NULL) == FALSE && (GetLastError() != ERROR_PIPE_CONNECTED))
    {
        std::cerr << "Failed to connect pipe " << name << std::endl;
        return false;
    }
    return true;
}

void PipeServer::disconnect()
{
    DisconnectNamedPipe(handle);
}

void PipeServer::close()
{
    DisconnectNamedPipe(handle);
    Pipe::close();
}

std::string Pipe::make_ID()
{
    return "\\\\.\\pipe\\" + name;
}

size_t Pipe::peek() const
{
    DWORD available_byte_count;
    if (PeekNamedPipe(handle, 0, 0, 0, &available_byte_count, 0) == false)
    {
        return 0;
    }
    return available_byte_count;
}

bool Pipe::opened() const
{
    return handle != INVALID_HANDLE_VALUE;
}

bool Pipe::write(const char* data, size_t size)
{
    DWORD bytes_written = 0;
    bool success = WriteFile(handle, data, (DWORD)size, &bytes_written, NULL);

    if (!success || bytes_written != size)
    {
        gbl_err << "Failed to write to pipe" << name << std::endl;
        return false;
    }
    return true;
}

bool Pipe::read(char* data, size_t size)
{
    DWORD bytes_read = 0;
    bool success = ReadFile(handle, data, (DWORD)size, &bytes_read, NULL);

    if (!success || bytes_read != size)
    {
        gbl_err << "Failed to read from pipe" << name << std::endl;
        return false;
    }

    return true;
}

bool Pipe::read_all(std::vector<char>& vec)
{
    constexpr size_t buff_size = 1024;
    DWORD bytes_read = 0;
    DWORD total_bytes_read = 0;
    std::list<std::vector<char>> out_segs;

    out_segs.push_back(std::vector<char>(buff_size));

    for (;;)
    {
        bool success = ReadFile(handle, out_segs.back().data(), buff_size, &bytes_read, NULL);
        if (!success) break;
        out_segs.back().resize(bytes_read);
        out_segs.back().shrink_to_fit();
        total_bytes_read += bytes_read;
    }
    vec.reserve(total_bytes_read);

    for (auto& seg : out_segs)
    {
        vec.insert(vec.end(), seg.begin(), seg.end());
    }
    return true;
}

bool Pipe::read_all(std::string& str)
{
    std::vector<char> vec;
    read_all(vec);
    str = std::string(vec.begin(), vec.end());
    return true;
}

void Pipe::close()
{
    CloseHandle(handle);
    handle = INVALID_HANDLE_VALUE;
}

Pipe::~Pipe()
{
    close();
}

bool PipeClient::open(const std::string& name)
{
    this->name = name;

    int attempts = 3;
    while (attempts --> 0)
    {
        if (WaitNamedPipeA(TEXT(make_ID().c_str()), NMPWAIT_USE_DEFAULT_WAIT) == false)
        {
            gbl_err << "Could not open pipe " << name << ": wait timed out";
            return false;
        }
        handle = CreateFileA(make_ID().c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

        if (handle != INVALID_HANDLE_VALUE)
        {
            break;
        }

        if (GetLastError() != ERROR_PIPE_BUSY)
        {
            gbl_err << "Could not open pipe " << name << std::endl;
            return false;
        }
    }
    if (attempts < 0)
    {
        gbl_err << "Failed to open pipe " << name << " after several attempts" << std::endl;
        return false;
    }

    //DWORD dwMode = PIPE_READMODE_MESSAGE;
    //bool success = SetNamedPipeHandleState(handle, &dwMode, NULL, NULL);
    //
    //if (!success)
    //{
    //    std::cout << "SetNamedPipeHandleState failed" << std::endl;
    //    return false;
    //}

    return attempts >= 0;
}