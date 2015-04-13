#pragma once

#include "ientityinspector.h"
#include <wx/event.h>

namespace ui
{

	namespace
	{
		const std::string DEF_HEAD_KEY = "def_head";
	}

class AIHeadPropertyEditor :
	public IPropertyEditor,
	public IPropertyEditorDialog,
	public wxEvtHandler
{
private:
	// The top-level widget
	wxPanel* _widget;

	Entity* _entity;

public:
	// Default constructor
	AIHeadPropertyEditor();

	~AIHeadPropertyEditor();

	wxPanel* getWidget();

	AIHeadPropertyEditor(wxWindow* parent, Entity* entity,
		const std::string& key, const std::string& options);

	IPropertyEditorPtr createNew(wxWindow* parent, Entity* entity,
								const std::string& key,
								const std::string& options);

	std::string runDialog(Entity* entity, const std::string& key);

private:
	void onChooseButton(wxCommandEvent& ev);
};

} // namespace ui
