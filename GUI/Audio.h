#pragma once
#include <Windows.h>
#include <string>

namespace Audio
{
    bool SetMute(DWORD process_ID, bool mute);
    bool SetMute(const std::string& process_name, bool mute);
};