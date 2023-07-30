#pragma once
#include <Windows.h>
#include <vector>
#include <string>

class ClipDataObject : public IDataObject
{
public:
	struct ObjectEntry
	{
		FORMATETC	format;
		STGMEDIUM	medium;
	};
	std::vector<ObjectEntry> entries;
	LONG ref_count;

	ClipDataObject(const std::vector<ObjectEntry>& entries);

	HRESULT	__stdcall QueryInterface(REFIID iid, void** objects) override;
	ULONG __stdcall	AddRef(void) override;
	ULONG __stdcall	Release(void) override;

	HRESULT	__stdcall GetData(FORMATETC* format, STGMEDIUM* medium) override;
	HRESULT	__stdcall GetDataHere(FORMATETC* format, STGMEDIUM* medium) override;
	HRESULT	__stdcall QueryGetData(FORMATETC* format) override;
	HRESULT	__stdcall GetCanonicalFormatEtc(FORMATETC* format_in, FORMATETC* format_out) override;
	HRESULT	__stdcall SetData(FORMATETC* format, STGMEDIUM* medium, BOOL release) override;
	HRESULT	__stdcall EnumFormatEtc(DWORD direction, IEnumFORMATETC** enum_formats) override;
	HRESULT	__stdcall DAdvise(FORMATETC* format, DWORD advf, IAdviseSink* advice_sink, DWORD* connection) override;
	HRESULT	__stdcall DUnadvise(DWORD connection) override;
	HRESULT	__stdcall EnumDAdvise(IEnumSTATDATA** enum_advise) override;

	HRESULT CloneMedium(STGMEDIUM& dst_medium, STGMEDIUM& src_medium, const FORMATETC& format);
};

