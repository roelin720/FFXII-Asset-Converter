#pragma once
#include <Windows.h>
#include "VBFArchive.h"
#include "ClipDropTarget.h"
#include "VBFClipHelper.h"

class ClipDropSource : public IDropSource
{
public:
	LONG ref_count;
	DWORD mk_button; //MK_LBUTTON or ML_RBUTTON
	ClipDropTarget* self_target = nullptr;
	VBFExtractClipHelper* vbf_helper = nullptr;

	ClipDropSource(DWORD mouse_button, ClipDropTarget* self_target = nullptr);

	virtual HRESULT __stdcall QueryInterface(REFIID iid, void** objects) override;
	virtual ULONG __stdcall AddRef(void) override;
	virtual ULONG __stdcall Release(void) override;

	// IUnknown functions
	virtual HRESULT _stdcall QueryContinueDrag(BOOL fEscapePressed, DWORD key_state) override;
	virtual HRESULT _stdcall GiveFeedback(DWORD dwEffect) override;

	bool TargetingSelf();
};