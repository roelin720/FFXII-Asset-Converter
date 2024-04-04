#include "ClipDropSource.h"
#include "ConverterInterface.h"
#include "GUIBrowser.h"

ClipDropSource::ClipDropSource(DWORD mouse_button, ClipDropTarget* self_target)
:	ref_count(1),
	mk_button(mouse_button),
	self_target(self_target)
{}

HRESULT __stdcall ClipDropSource::QueryInterface(REFIID iid, void** objects)
{
	*objects = NULL;
	if (iid == IID_IDropSource || iid == IID_IUnknown)
	{
		*objects = this;
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

ULONG __stdcall ClipDropSource::AddRef(void)
{
	return InterlockedIncrement(&ref_count);
}

ULONG __stdcall ClipDropSource::Release(void)
{
	LONG new_ref_count = InterlockedDecrement(&ref_count);

	if (new_ref_count == 0)
	{
		delete this;
	}

	return new_ref_count;
}

HRESULT _stdcall ClipDropSource::QueryContinueDrag(BOOL fEscapePressed, DWORD key_state)
{
	if (key_state & (mk_button ^ 3) || fEscapePressed)
	{
		LOG(INFO) << "Cancelling drag-drop" << std::endl;
		return DRAGDROP_S_CANCEL;
	}

	if ((key_state & mk_button) == 0)
	{
		if (TargetingSelf())
		{
			return DRAGDROP_S_CANCEL;
		}
		if (vbf_helper && IO::Join(vbf_helper->browser.cur_parent_folder).contains(".vbf"))
		{
			if (!vbf_helper->perform_extractions())
			{
				return DRAGDROP_S_CANCEL;
			}
		}

		return DRAGDROP_S_DROP;
	}

	return S_OK;
}

HRESULT _stdcall ClipDropSource::GiveFeedback(DWORD dwEffect)
{
	return DRAGDROP_S_USEDEFAULTCURSORS;
}

bool ClipDropSource::TargetingSelf()
{
	if (self_target && self_target->dragging_over)
	{
		return true;
	}
	return false;
}
