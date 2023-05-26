#pragma once
#include <windows.h>
#include <string>

struct Audio
{
    static bool SetMute(DWORD process_ID, bool mute);
    static bool SetMute(const std::string& process_name, bool mute);
};