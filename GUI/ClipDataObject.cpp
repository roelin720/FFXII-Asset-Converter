#include "ClipDataObject.h"
#include <ShlObj.h>

ClipDataObject::ClipDataObject(const std::vector<ObjectEntry>& entries)
	: ref_count(1), entries(entries)
{}

HRESULT __stdcall ClipDataObject::QueryInterface(REFIID iid, void** objects)
{
	if (objects == NULL)
	{
		return E_POINTER;
	}

	*objects = NULL;
	if (iid == IID_IDataObject || iid == IID_IUnknown)
	{
		*objects = this;
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

ULONG __stdcall ClipDataObject::AddRef(void)
{
	return InterlockedIncrement(&ref_count);
}

ULONG __stdcall ClipDataObject::Release(void)
{
	LONG new_ref_count = InterlockedDecrement(&ref_count);

	if (new_ref_count == 0)
	{
		for (auto& entry : entries)
		{
			ReleaseStgMedium(&entry.medium);
		}
		delete this;
	}
	return new_ref_count;
}

HRESULT __stdcall ClipDataObject::GetData(FORMATETC* format, STGMEDIUM* medium)
{
	if (format == NULL || medium == NULL)
	{
		return E_INVALIDARG;
	}

	if (QueryGetData(format) == DV_E_FORMATETC)
	{
		return DV_E_FORMATETC;
	}

	for (auto& entry : entries)
	{
		if (entry.format.cfFormat == format->cfFormat &&
			entry.format.tymed & format->tymed &&
			entry.format.dwAspect == format->dwAspect)
		{
			return CloneMedium(*medium, entry.medium, entry.format);
		}
	}

	return DV_E_FORMATETC;
}

HRESULT ClipDataObject::CloneMedium(STGMEDIUM& dst, STGMEDIUM& src, const FORMATETC& format)
{
	switch (format.tymed)
	{
	case TYMED_NULL: 
		break;
	case TYMED_ISTREAM:
		dst.pstm = src.pstm;
		src.pstm->AddRef();
		break;
	case TYMED_ISTORAGE:
		dst.pstg = src.pstg;
		src.pstg->AddRef();
		break;
	default:
		HANDLE clone = OleDuplicateData(src.hGlobal, format.cfFormat, NULL);
		if (clone == NULL)
		{
			return E_UNEXPECTED;
		}
		switch (format.tymed)
		{
		case TYMED_HGLOBAL:	dst.hGlobal			= HGLOBAL(clone);		break;
		case TYMED_FILE:	dst.lpszFileName	= LPOLESTR(clone);		break;
		case TYMED_GDI:		dst.hBitmap			= HBITMAP(clone);		break;
		case TYMED_MFPICT:	dst.hMetaFilePict	= HMETAFILEPICT(clone);	break;
		case TYMED_ENHMF:	dst.hEnhMetaFile	= HENHMETAFILE(clone);	break;
		}
	}

	dst.tymed = src.tymed;
	dst.pUnkForRelease = src.pUnkForRelease;
	if (dst.pUnkForRelease != NULL)
	{
		dst.pUnkForRelease->AddRef();
	}

	return S_OK;
}

HRESULT __stdcall ClipDataObject::GetDataHere(FORMATETC* format, STGMEDIUM* medium)
{
	return DV_E_TYMED;
}

HRESULT	__stdcall ClipDataObject::QueryGetData(FORMATETC* format)
{
	if (format == NULL)
	{
		return E_INVALIDARG;
	}

	for (auto& entry : entries)
	{
		if (entry.format.cfFormat == format->cfFormat &&
			entry.format.tymed & format->tymed &&
			entry.format.dwAspect == format->dwAspect)
		{
			return S_OK;
		}
	}

	return DV_E_FORMATETC;
}

HRESULT __stdcall ClipDataObject::GetCanonicalFormatEtc(FORMATETC* format_in, FORMATETC* format_out)
{
	if (format_out == NULL)
	{
		return E_INVALIDARG;
	}
	format_out->ptd = NULL;

	return E_NOTIMPL;
}

HRESULT __stdcall ClipDataObject::SetData(FORMATETC* format, STGMEDIUM* medium, BOOL release)
{
	if (format == NULL || medium == NULL)
	{
		return E_INVALIDARG;
	}

	ObjectEntry entry { .format = *format };

	if (release)
	{
		entry.medium = *medium;
	}
	else
	{
		if (HRESULT result = CloneMedium(entry.medium, *medium, *format); result != S_OK)
		{
			return result;
		}
	}

	entries.push_back(entry);
	return S_OK;
}

HRESULT __stdcall ClipDataObject::EnumFormatEtc(DWORD direction, IEnumFORMATETC** ppEnumFormatEtc)
{
	if (ppEnumFormatEtc == NULL)
	{
		return E_INVALIDARG;
	}

	if (direction == DATADIR_GET)
	{
		FORMATETC formats[] =
		{
			{ CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL },
		};
		return SHCreateStdEnumFmtEtc(_countof(formats), formats, ppEnumFormatEtc);
	}
	return E_NOTIMPL;
}

HRESULT __stdcall ClipDataObject::DAdvise(FORMATETC* format, DWORD advf, IAdviseSink* advice_sink, DWORD* connection)
{
	return E_NOTIMPL;
}

HRESULT __stdcall ClipDataObject::DUnadvise(DWORD connection)
{
	return OLE_E_ADVISENOTSUPPORTED;
}

HRESULT __stdcall ClipDataObject::EnumDAdvise(IEnumSTATDATA** enum_advise)
{
	return OLE_E_ADVISENOTSUPPORTED;
}