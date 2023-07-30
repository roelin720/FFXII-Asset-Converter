#pragma once
#include <Windows.h>
#include "VBFClipHelper.h"

class FileBrowser;
class ClipDropTarget : public IDropTarget {
public:
    LONG ref_count = 1;
    bool dragging_over = false;
    FileBrowser& browser;
    VBFInjectClipHelper* vbf_helper = nullptr;

    ClipDropTarget(FileBrowser& browser);

    std::vector<std::wstring> GetFilesFromDataObject(IDataObject* object);

    HRESULT __stdcall DragEnter(IDataObject* object, DWORD key_state, POINTL point, DWORD* drop_effect) override;
    HRESULT __stdcall DragOver(DWORD key_state, POINTL point, DWORD* drop_effect) override;
    HRESULT __stdcall DragLeave() override;
    HRESULT __stdcall Drop(IDataObject* object, DWORD key_state, POINTL point, DWORD* drop_effect) override;

    //IUnknown functions
    HRESULT __stdcall QueryInterface(REFIID riid, void** objects);
    ULONG __stdcall AddRef();
    ULONG __stdcall Release();

    DWORD GetDropEffect(DWORD key_state, POINTL point);
};