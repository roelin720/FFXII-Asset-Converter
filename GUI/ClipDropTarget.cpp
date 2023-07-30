#include "ClipDropTarget.h"
#include "GUIBrowser.h"
#include <ShlObj.h>
#include <locale>
#include <codecvt>
#include <Shlwapi.h>

std::vector<std::wstring> ClipDropTarget::GetFilesFromDataObject(IDataObject* object)
{
	std::vector<std::wstring> file_names;
	FORMATETC format = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	STGMEDIUM medium = {};

	if (SUCCEEDED(object->GetData(&format, &medium))) 
	{
		HDROP hDrop = static_cast<HDROP>(medium.hGlobal);
		UINT numFiles = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);

		for (UINT i = 0; i < numFiles; ++i) 
		{
			UINT file_name_len = DragQueryFileW(hDrop, i, nullptr, 0);
			if (file_name_len > 0) 
			{
				std::wstring file_name;

				file_name.resize(file_name_len + 1);
				DragQueryFileW(hDrop, i, file_name.data(), file_name_len + 1); 
				file_name.resize(file_name_len);

				file_names.push_back(file_name);
			}
		}

		ReleaseStgMedium(&medium);
	}

	return file_names;
}


ClipDropTarget::ClipDropTarget(FileBrowser& browser) : ref_count(1), dragging_over(false), browser(browser) {}

HRESULT __stdcall ClipDropTarget::DragEnter(IDataObject* object, DWORD key_state, POINTL point, DWORD* drop_effect)
{
	dragging_over = true;
	*drop_effect = GetDropEffect(key_state, point);
	return S_OK;
}

HRESULT __stdcall ClipDropTarget::DragOver(DWORD key_state, POINTL point, DWORD* drop_effect)
{
	dragging_over = true;
	*drop_effect = GetDropEffect(key_state, point);
	return S_OK;
}

HRESULT __stdcall ClipDropTarget::DragLeave()
{
	dragging_over = false;
	return S_OK;
}

HRESULT __stdcall ClipDropTarget::Drop(IDataObject* object, DWORD key_state, POINTL point, DWORD* drop_effect)
{
	std::string cur_folder_str = IO::Join(browser.cur_parent_folder);
	if (cur_folder_str.contains(".vbf"))
	{
		std::vector<std::wstring> dropped_paths = GetFilesFromDataObject(object);

		if (!vbf_helper|| !vbf_helper->perform_injections(dropped_paths))
		{
			return E_UNEXPECTED;
		}
	}
	else
	{
		std::wstring canonical_folder_str = std::filesystem::weakly_canonical(std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(cur_folder_str)).wstring();
		canonical_folder_str.push_back('\0');

		*drop_effect = GetDropEffect(key_state, point);

		if (*drop_effect & (DROPEFFECT_COPY | DROPEFFECT_MOVE)) 
		{
			std::vector<std::wstring> dropped_paths = GetFilesFromDataObject(object);
			std::wstring path_buffer;

			for (std::wstring path : dropped_paths)
			{
				path_buffer += path;
				path_buffer.push_back('\0');
			}
			path_buffer.push_back('\0');

			SHFILEOPSTRUCTW sh_op = { 0 };
			sh_op.wFunc = (*drop_effect & DROPEFFECT_MOVE) ? FO_MOVE : FO_COPY;
			sh_op.pFrom = path_buffer.c_str();
			sh_op.pTo = canonical_folder_str.c_str();
			sh_op.fFlags = FOF_ALLOWUNDO;
			SHFileOperationW(&sh_op);
		}
		else return E_UNEXPECTED;
	}
	return S_OK;
}

HRESULT __stdcall ClipDropTarget::QueryInterface(REFIID riid, void** objects)
{
	if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDropTarget)) 
	{
		*objects = static_cast<IDropTarget*>(this);
		AddRef();
		return S_OK;
	}
	*objects = nullptr;
	return E_NOINTERFACE;
}

ULONG __stdcall ClipDropTarget::AddRef()
{
	return InterlockedIncrement(&ref_count);
}

ULONG __stdcall ClipDropTarget::Release()
{
	ULONG new_ref_count = InterlockedDecrement(&ref_count);
	if (new_ref_count == 0) 
	{
		delete this;
	}
	return new_ref_count;
}

DWORD ClipDropTarget::GetDropEffect(DWORD key_state, POINTL point)
{
	return key_state & MK_SHIFT ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
}

