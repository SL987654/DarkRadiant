#include "MapInfoDialog.h"

#include "i18n.h"
#include "ieventmanager.h"
#include "imainframe.h"
#include "iuimanager.h"

#include "EntityInfoTab.h"
#include "ShaderInfoTab.h"
#include "ModelInfoTab.h"

#include <wx/artprov.h>
#include <wx/sizer.h>

namespace ui
{

namespace
{
	const int MAPINFO_DEFAULT_SIZE_X = 600;
	const int MAPINFO_DEFAULT_SIZE_Y = 550;
	const char* const MAPINFO_WINDOW_TITLE = N_("Map Info");
}

MapInfoDialog::MapInfoDialog() :
	DialogBase(_(MAPINFO_WINDOW_TITLE))
{
	SetSize(MAPINFO_DEFAULT_SIZE_X, MAPINFO_DEFAULT_SIZE_Y);

	// Create all the widgets
	populateWindow();
}


void MapInfoDialog::populateWindow()
{
	_imageList.reset(new wxImageList(16, 16));
	
	SetSizer(new wxBoxSizer(wxVERTICAL));
	
	_notebook = new wxNotebook(this, wxID_ANY);
	_notebook->SetImageList(_imageList.get());

	GetSizer()->Add(_notebook, 1, wxEXPAND | wxALL, 12);
	GetSizer()->Add(CreateStdDialogButtonSizer(wxCLOSE), 0, wxALIGN_RIGHT | wxALL, 12);

	SetAffirmativeId(wxID_CLOSE);

	EntityInfoTab* entityTab = new EntityInfoTab(_notebook);
	addTab(entityTab, entityTab->getLabel(), entityTab->getIconName());

	ModelInfoTab* modelTab = new ModelInfoTab(_notebook);
	addTab(modelTab, modelTab->getLabel(), modelTab->getIconName());

	ShaderInfoTab* shaderTab = new ShaderInfoTab(_notebook);
	addTab(shaderTab, shaderTab->getLabel(), shaderTab->getIconName());
}

void MapInfoDialog::addTab(wxWindow* panel, const std::string& label, const std::string& icon)
{
	// Load the icon
	int imageId = icon.empty() ? -1 : 
		_imageList->Add(wxArtProvider::GetBitmap(GlobalUIManager().ArtIdPrefix() + icon));
	
	panel->Reparent(_notebook);
	_notebook->AddPage(panel, label, false, imageId);
}

void MapInfoDialog::ShowDialog(const cmd::ArgumentList& args)
{
	MapInfoDialog* dialog = new MapInfoDialog;

	dialog->ShowModal();
	dialog->Destroy();
}

} // namespace ui
