#pragma once

#include "ClassEditor.h"
#include "wxutil/XmlResourceBasedWidget.h"
#include "wxutil/menu/PopupMenu.h"
#include <memory>

class wxCheckBox;
class wxStaticText;
class wxSpinCtrl;
class wxSpinCtrlDouble;
class wxMenu;

namespace ui
{

class StimEditor :
	public ClassEditor,
	private wxutil::XmlResourceBasedWidget
{
private:
	struct PropertyWidgets
	{
		wxCheckBox* active;
		wxCheckBox* useBounds;
		wxCheckBox* radiusToggle;
		wxSpinCtrl* radiusEntry;
		wxCheckBox* finalRadiusToggle;
		wxSpinCtrl* finalRadiusEntry;
		wxCheckBox* timeIntToggle;
		wxSpinCtrl* timeIntEntry;
		wxStaticText* timeUnitLabel;

		struct TimerWidgets
		{
			wxCheckBox* toggle;
			wxPanel* entryHBox;
			wxSpinCtrl* hour;
			wxSpinCtrl* minute;
			wxSpinCtrl* second;
			wxSpinCtrl* millisecond;

			wxCheckBox* typeToggle;

			wxCheckBox* reloadToggle;
			wxSpinCtrl* reloadEntry;
			wxStaticText* reloadLabel;
			wxPanel* reloadHBox;

			wxCheckBox* waitToggle;
		} timer;

		wxCheckBox* durationToggle;
		wxSpinCtrl* durationEntry;
		wxStaticText* durationUnitLabel;
		wxCheckBox* maxFireCountToggle;
		wxSpinCtrl* maxFireCountEntry;
		wxCheckBox* magnToggle;
		wxSpinCtrl* magnEntry;
		wxCheckBox* falloffToggle;
		wxSpinCtrlDouble* falloffEntry;
		wxCheckBox* chanceToggle;
		wxSpinCtrlDouble* chanceEntry;
		wxCheckBox* velocityToggle;
		wxTextCtrl* velocityEntry;

		struct BoundsWidgets
		{
			wxCheckBox* toggle;
			wxPanel* panel;
			wxStaticText* minLabel;
			wxTextCtrl* minEntry;
			wxStaticText* maxLabel;
			wxTextCtrl* maxEntry;
		} bounds;
	} _propertyWidgets;

	struct ListContextMenu
	{
		std::unique_ptr<wxMenu> menu;
		wxMenuItem* remove;
		wxMenuItem* add;
		wxMenuItem* enable;
		wxMenuItem* disable;
		wxMenuItem* duplicate;
	} _contextMenu;

public:
	/** greebo: Constructor creates all the widgets
	 */
	StimEditor(wxWindow* parent, StimTypes& stimTypes);

	/** greebo: Sets the new entity (is called by the StimResponseEditor class)
	 */
	virtual void setEntity(const SREntityPtr& entity);

	/** greebo: Updates the widgets (e.g. after a selection change)
	 */
	void update();

private:
	/** greebo: Retrieves the formatted timer string h:m:s:ms
	 */
	std::string getTimerString();

	/** greebo: Adds a new stim to the list
	 */
	void addSR();

	/** greebo: Gets called when a spinbutton changes, overrides the
	 * 			method from the base class.
	 */
	void spinButtonChanged(wxSpinCtrl* ctrl);

	/** greebo: Updates the associated text fields when a check box
	 * 			is toggled.
	 */
	void checkBoxToggled(wxCheckBox* toggleButton);

	/** greebo: As the name states, this creates the context menu widgets.
	 */
	void createContextMenu();

	/** greebo: Gets called when the stim selection gets changed
	 */
	virtual void selectionChanged();

	void openContextMenu(wxutil::TreeView* view);

	/** greebo: Creates all the widgets
	 */
	void populatePage(wxWindow* parent);

	// Connects signals
	void setupEditingPanel();

	// Context menu callbacks
	void onContextMenuAdd(wxCommandEvent& ev);
	void onContextMenuDelete(wxCommandEvent& ev);
};

} // namespace ui
