#include "Audio.h"
#include "ConverterInterface.h"
#include <iostream>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <Audiopolicy.h>
#include <comdef.h>
#include <comip.h>
#include <cstdio>
#include <tlhelp32.h>

#define CHECK(x) {HRESULT hr = x; if(hr != S_OK) { _com_error err(hr); LOG(ERR) << "SET MUTE AUDIO FAILURE - " #x << " FAILED - " << err.ErrorMessage() << std::endl; return 0; }}
#define SOFT_CHECK(x) {HRESULT hr = x; if(hr != S_OK) { continue; }}

_COM_SMARTPTR_TYPEDEF(IMMDevice, __uuidof(IMMDevice));
_COM_SMARTPTR_TYPEDEF(IMMDeviceEnumerator, __uuidof(IMMDeviceEnumerator));
_COM_SMARTPTR_TYPEDEF(IAudioSessionManager2, __uuidof(IAudioSessionManager2));
_COM_SMARTPTR_TYPEDEF(IAudioSessionManager2, __uuidof(IAudioSessionManager2));
_COM_SMARTPTR_TYPEDEF(IAudioSessionEnumerator, __uuidof(IAudioSessionEnumerator));
_COM_SMARTPTR_TYPEDEF(IAudioSessionControl2, __uuidof(IAudioSessionControl2));
_COM_SMARTPTR_TYPEDEF(IAudioSessionControl, __uuidof(IAudioSessionControl));
_COM_SMARTPTR_TYPEDEF(ISimpleAudioVolume, __uuidof(ISimpleAudioVolume));

bool Audio::SetMute(DWORD processId, bool mute)
{
    IMMDevicePtr pDevice;
    IMMDeviceEnumeratorPtr pEnumerator;
    IAudioSessionManager2Ptr mgr;

    CHECK(CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator));
    CHECK(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice));
    CHECK(pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&mgr));
    IAudioSessionEnumeratorPtr enumerator;
    CHECK(mgr->GetSessionEnumerator(&enumerator));
    int sessionCount;
    CHECK(enumerator->GetCount(&sessionCount));
    for (int i = 0; i < sessionCount; i++)
    {
        IAudioSessionControlPtr control;
        SOFT_CHECK(enumerator->GetSession(i, &control))
        IAudioSessionControl2Ptr control2;
        SOFT_CHECK(control->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&control2));
        DWORD foundProcessId;
        SOFT_CHECK(control2->GetProcessId(&foundProcessId));
        if (foundProcessId == processId)
        {
            ISimpleAudioVolumePtr volume;
            CHECK(control2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&volume));
            CHECK(volume->SetMute(mute, 0));
            return true;
        }
    }
    return false;
}

bool Audio::SetMute(const std::string& process_name, bool mute)
{
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (Process32First(snapshot, &entry) == TRUE)
    {
        while (Process32Next(snapshot, &entry) == TRUE)
        {
            if (stricmp(entry.szExeFile, process_name.c_str()) == 0)
            {
                bool result = SetMute(entry.th32ProcessID, mute);
                CloseHandle(snapshot);
                return result;
            }
        }
    }

    CloseHandle(snapshot);

    return false;
}
