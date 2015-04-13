#include "FilterMenu.h"

#include "iuimanager.h"
#include "string/convert.h"
#include "i18n.h"
#include <wx/menu.h>

namespace ui
{

namespace
{
	// These are used for the general-purpose Filter Menu:
	const char* const FILTERS_MENU_BAR = "filters";
	const char* const FILTERS_MENU_FOLDER = "allfilters";
	const char* const FILTERS_MENU_CAPTION = N_("_Filters");

	const char* const MENU_ICON = "iconFilter16.png";
}

std::size_t FilterMenu::_counter = 0;

FilterMenu::FilterMenu()
{
	IMenuManager& menuManager = GlobalUIManager().getMenuManager();

	// Create a unique name for the menu
	_path = FILTERS_MENU_BAR + string::to_string(_counter++);

	// Menu not yet constructed, do it now
	_menu = dynamic_cast<wxMenu*>(menuManager.add("", _path,
					menuFolder, _(FILTERS_MENU_CAPTION), "", ""));
	assert(_menu != NULL);

	_targetPath = _path;

	// Visit the filters in the FilterSystem to populate the menu
	GlobalFilterSystem().forEachFilter(*this);
}

FilterMenu::~FilterMenu()
{
	GlobalUIManager().getMenuManager().remove(_path);
}

// Visitor function
void FilterMenu::visit(const std::string& filterName)
{
	// Get the menu manager
	IMenuManager& menuManager = GlobalUIManager().getMenuManager();

	std::string eventName = GlobalFilterSystem().getFilterEventName(filterName);

	// Create the menu item
	menuManager.add(_targetPath, _targetPath + "_" + filterName,
					menuItem, filterName,
					MENU_ICON, eventName);
}

wxMenu* FilterMenu::getMenuWidget()
{
	return _menu;
}

} // namespace
